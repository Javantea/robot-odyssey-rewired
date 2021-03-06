#include <emscripten.h>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include "sbt86.h"
#include "hardware.h"


OutputQueue::OutputQueue()
{
    clear();
}

void OutputQueue::clear()
{
    items.clear();
    frames.clear();
    skipDelay();
}

void OutputQueue::skipDelay()
{
    delay_remaining = 0;
    while (!items.empty() && items.front().otype == OUT_DELAY) {
        items.pop_front();
    }
}

void OutputQueue::pushFrameCGA(SBTStack *stack, uint8_t *framebuffer)
{
    if (frames.full() || items.full()) {
        stack->trace();
        assert(0 && "Frame queue is too deep! Infinite loop likely.");
        return;
    }

    OutputItem item;
    item.otype = OUT_CGA_FRAME;
    items.push_back(item);

    frames.push_back(*(CGAFramebuffer*) framebuffer);
}

void OutputQueue::pushFrameRGB(SBTStack *stack, uint32_t *framebuffer)
{
    if (items.full()) {
        stack->trace();
        assert(0 && "Frame queue is too deep! Infinite loop likely.");
        return;
    }

    OutputItem item;
    item.otype = OUT_RGB_FRAME;
    items.push_back(item);

    // RGB frames aren't queued, there's only one shared buffer.
    memcpy(rgb_pixels, framebuffer, sizeof rgb_pixels);
}

void OutputQueue::pushDelay(uint32_t millis)
{
    if (!items.full()) {
        OutputItem item;
        item.otype = OUT_DELAY;
        item.u.delay = millis;
        items.push_back(item);
    }
}

void OutputQueue::pushSpeakerTimestamp(uint32_t timestamp)
{
    if (items.full()) {
        assert(0 && "Speaker queue is too deep! Infinite loop likely.");
        return;
    }

    OutputItem item;
    item.otype = OUT_SPEAKER_TIMESTAMP;
    item.u.timestamp = timestamp;
    items.push_back(item);
}

void OutputQueue::dequeueCGAFrame()
{
    assert(!frames.empty());
    CGAFramebuffer &frame = frames.front();

    // Expand CGA color to RGBA
    for (unsigned plane = 0; plane < 2; plane++) {
        for (unsigned y=0; y < CGAFramebuffer::HEIGHT/2; y++) {
            uint32_t *rgb_line = rgb_pixels + (y*2+plane)*SCREEN_WIDTH*CGAFramebuffer::ZOOM;

            for (unsigned x=0; x < CGAFramebuffer::WIDTH; x++) {
                unsigned byte = 0x2000*plane + (x + CGAFramebuffer::WIDTH*y)/4;
                unsigned bit = 3 - (x % 4);
                unsigned color = 0x3 & (frame.bytes[byte] >> (bit * 2));
                uint32_t rgb = cga_palette[color];

                // Zoom each CGA pixel
                for (unsigned zy=0; zy<CGAFramebuffer::ZOOM; zy++) {
                    for (unsigned zx=0; zx<CGAFramebuffer::ZOOM; zx++) {
                        rgb_line[zx + zy*SCREEN_WIDTH] = rgb;
                    }
                }

                rgb_line += CGAFramebuffer::ZOOM;
            }
        }
    }

    // Free the ring buffer slot we were using
    frames.pop_front();
}

void OutputQueue::renderFrame()
{
    // Synchronously ask Javascript to do something with this array
    EM_ASM_({
        Module.onRenderFrame(HEAPU8.subarray($0, $1));
    }, rgb_pixels, rgb_pixels + (SCREEN_WIDTH * SCREEN_HEIGHT));
}

uint32_t OutputQueue::renderSoundEffect(uint32_t first_timestamp)
{
    // Starting at the indicated timestamp and from the current output
    // queue position, slurp up all subsequent audio events and generate
    // a single PCM sound effect. Returns the delay, in milliseconds,
    // to insert concurrently with sound playback.

    // The first sample at index zero is always a "1", then the delay
    // we load from the output queue tells us where to put a "0", then
    // where to put the next "1", etc.

    uint32_t previous_timestamp = first_timestamp;
    int8_t next_sample = 1;
    uint32_t sample_count = 0;
    int32_t clocks_remaining = 0;

    while (sample_count < AUDIO_BUFFER_SAMPLES && clocks_remaining >= 0) {

        pcm_samples[sample_count] = next_sample;
        clocks_remaining -= CPU_CLOCKS_PER_SAMPLE;
        sample_count++;

        if (clocks_remaining < 0) {
            if (items.empty() || items.front().otype != OUT_SPEAKER_TIMESTAMP) {
                break;
            }
            uint32_t timestamp = items.front().u.timestamp;
            items.pop_front();
            clocks_remaining += timestamp - previous_timestamp;
            previous_timestamp = timestamp;
            next_sample = !next_sample;
        }
    }

    // Synchronously copy out the buffer and queue it for rendering, in Javascript
    EM_ASM_({
        Module.onRenderSound(HEAP8.subarray($0, $0 + $1), $2);
    }, pcm_samples, sample_count, AUDIO_HZ);

    // Return a corresponding delay in milliseconds, rounding up.
    return (sample_count * 1000 + AUDIO_HZ - 1) / AUDIO_HZ;
}

uint32_t OutputQueue::run()
{
    // Generate output until the queue is empty (returning zero) or
    // a delay (returning a nonzero number of milliseconds)

    while (true) {
        // Split up large delays
        static const uint32_t max_delay_per_step = 100;
        if (delay_remaining > 0) {
            uint32_t delay = std::min(delay_remaining, max_delay_per_step);
            delay_remaining -= delay;
            return delay;
        }

        // No more delay.. now we need more output, make sure the queue has items
        if (items.empty()) {
            return 0;
        }

        OutputItem item = items.front();
        items.pop_front();

        switch (item.otype) {

            case OUT_CGA_FRAME:
                dequeueCGAFrame();
                renderFrame();
                break;

            case OUT_RGB_FRAME:
                renderFrame();
                break;

            case OUT_DELAY:
                delay_remaining += item.u.delay;
                break;

            case OUT_SPEAKER_TIMESTAMP:
                delay_remaining += renderSoundEffect(item.u.timestamp);
                break;
        }
    }
}
