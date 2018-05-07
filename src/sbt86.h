/* -*- Mode: C++; c-basic-offset: 4 -*-
 *
 * Public header for SBT86. Defines common data types for binary
 * translated programs, and the SBTProcess class.
 *
 * SBTProcess is a binary-translated "process", a sandboxed virtual
 * machine with an isolated address space. SBTProcess itself is an
 * abstract base class. SBT86 generates code for subclasses of
 * SBTProcess.
 *
 * Copyright (c) 2009 Micah Elizabeth Scott <micah@scanlime.org>
 *
 *    Permission is hereby granted, free of charge, to any person
 *    obtaining a copy of this software and associated documentation
 *    files (the "Software"), to deal in the Software without
 *    restriction, including without limitation the rights to use,
 *    copy, modify, merge, publish, distribute, sublicense, and/or sell
 *    copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following
 *    conditions:
 *
 *    The above copyright notice and this permission notice shall be
 *    included in all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *    OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <stdint.h>
#include <setjmp.h>
#include <assert.h>

#define SBT_INLINE inline __attribute__((always_inline))

/*
 * Forward declarations
 */

class SBTHardware;
class SBTRegs;
class SBTStack;
class SBTProcess;
class SBTSegmentCache;


/*
 * SBT_SAVE_FILE_NAME --
 *
 *    Special filename we patch in for load/save operations
 */

#define SBT_SAVE_FILE_NAME "savefile"


/*
 * SBTAddressId --
 *
 *    These are addresses which can be determined statically by an
 *    SBT86 script, then looked up at runtime via SBTProcess.
 */

enum SBTAddressId {
    SBTADDR_WORLD_DATA,
    SBTADDR_CIRCUIT_DATA,
    SBTADDR_ROBOT_DATA_MAIN,
    SBTADDR_ROBOT_DATA_GRABBER,
};


/*
 * SBTHardware --
 *
 *    Abstract base class for a virtual hardware implementation which
 *    backs an SBTProcess. Calls to the process's SBTHardware
 *    implementation are generated by SBT86 itself, and this is a good
 *    place for SBT86 hooks to call into.
 */

class SBTHardware
{
 public:
    static const int CLOCK_HZ = 4770000;

    static const uint32_t MEM_SIZE = 256 * 1024;
    uint8_t mem[MEM_SIZE];

    /*
     * Return a pointer to an emulated memory segment. Only 64K of
     * memory past the returned pointer is guaranteed to be valid.  If
     * the address is out of range, returns NULL.
     */
    uint8_t *memSeg(uint16_t seg);

    /*
     * Memory access utilities.
     */

    uint8_t peek8(uint16_t seg, uint16_t off);
    void poke8(uint16_t seg, uint16_t off, uint8_t value);
    int16_t peek16(uint16_t seg, uint16_t off);
    void poke16(uint16_t seg, uint16_t off, uint16_t value);

    /*
     * The highest normal segment we can support.  Any segments over
     * MAX_SEGMENT get remapped to MAX_SEGMENT. This serves two
     * purposes:
     *
     *   1. It makes reads from the BIOS harmless.
     *   2. It puts the CGA framebuffer somewhere useful.
     */
    static const uint32_t MAX_SEGMENT = (MEM_SIZE - 0x10000) >> 4;

    /*
     * Virtual hardware, used only for sound
     */
    virtual uint8_t in(uint16_t port, uint32_t timestamp) = 0;
    virtual void out(uint16_t port, uint8_t value, uint32_t timestamp) = 0;

    /*
     * Emulated interrupt handlers for DOS and BIOS
     */
    virtual SBTRegs interrupt10(SBTRegs reg, SBTStack *stack) = 0;
    virtual SBTRegs interrupt16(SBTRegs reg, SBTStack *stack) = 0;
    virtual SBTRegs interrupt21(SBTRegs reg, SBTStack *stack) = 0;

    /*
     * Hooks
     */
    virtual void outputFrame(SBTStack *stack, uint8_t *framebuffer) = 0;
    virtual void outputDelay(uint32_t millis) = 0;    
    virtual void exec(const char *program, const char *args) = 0;
    virtual void clearKeyboardBuffer() = 0;
};


/*
 * SBTRegs --
 *
 *    Register state for the virtual 8086 processor.
 */

class SBTRegs
{
 public:
    union {
        uint16_t ax;
        struct {
            uint8_t al, ah;
        };
    };
    union {
        uint16_t bx;
        struct {
            uint8_t bl, bh;
        };
    };
    union {
        uint16_t cx;
        struct {
            uint8_t cl, ch;
        };
    };
    union {
        uint16_t dx;
        struct {
            uint8_t dl, dh;
        };
    };
    uint16_t si, di;
    uint16_t cs, ds, es, ss;
    uint16_t bp, sp;

    /*
     * We cheat enormously on implementing 8086 flags: Instead of
     * calculating the flags for every ALU instruction, we store a
     * 32-bit version of that instruction's result.  All flag tests
     * are rewritten in terms of this result word. Anything that
     * explicitly sets flags does so by tweaking this result word in
     * such a way as to change the flag value we would calculate.
     *
     * To avoid having to store the word width separately, all 8-bit
     * results are left-shifted by 8.
     */
    uint32_t uresult;
    int32_t sresult;

    /*
     * Inline accessors for manually setting/inspecting flags.
     */

    SBT_INLINE bool getZF() {
        return (uresult & 0xFFFF) == 0;
    }
    SBT_INLINE bool getSF() {
        return (uresult & 0x8000) != 0;
    }
    SBT_INLINE bool getOF() {
        return (((sresult >> 1) ^ (sresult)) & 0x8000) != 0;
    }
    SBT_INLINE bool getCF() {
        return (uresult & 0x10000) != 0;
    }

    SBT_INLINE void setZF() {
        uresult &= ~0xFFFF;
    }
    SBT_INLINE void clearZF() {
        uresult |= 1;
    }
    SBT_INLINE void setOF() {
        sresult = 0x8000;
    }
    SBT_INLINE void clearOF() {
        sresult = 0;
    }
    SBT_INLINE void setCF() {
        uresult |= 0x10000;
    }
    SBT_INLINE void clearCF() {
        uresult &= 0xFFFF;
    }

    SBT_INLINE uint32_t saveCF() {
        return uresult & 0x10000;
    }
    SBT_INLINE void restoreCF(uint32_t saved) {
        uresult = (uresult & 0xFFFF) | saved;
    }
};


/*
 * SBTStack --
 *
 *    The virtual stack used by a translated SBT86 process. This
 *    cheats significantly, and the emulated stack is actually not
 *    part of the normal address space. The stack is also strongly
 *    typed, in order to catch translated code which breaks our
 *    assumptions.
 */

class SBTStack
{
 public:
    SBTStack();
    void reset();

    void pushw(uint16_t word);
    void pushf(SBTRegs reg);
    void pushret(uint16_t offset);

    uint16_t popw();
    SBTRegs popf(SBTRegs reg);    
    void popret(uint16_t offset);
    
    void trace();

    void preSaveRet();
    void postRestoreRet();

 private:
    enum Tag {
        STACK_TAG_INVALID = 0,
        STACK_TAG_WORD,
        STACK_TAG_FLAGS,
        STACK_TAG_RETADDR,
    };

    static const uint32_t STACK_SIZE = 512;
    static const uint16_t RET_VERIFICATION = 0xBEEF;
    uint32_t top;
    uint32_t total_calls_made;

    Tag tags[STACK_SIZE];
    uint32_t words[STACK_SIZE];
    uint16_t fn_addrs[STACK_SIZE];
    struct {
        uint32_t uresult;
        int32_t sresult;
    } flags[STACK_SIZE];
};


/*
 * SBTProcess --
 *
 *    An abstract base class to contain the state for one translated process.
 */

class SBTProcess
{
 public:
    /*
     * Prepare an instance of this process to execute. This zeroes
     * memory, and resets the program counter. The process will begin
     * executing from the entry point on the next run().
     *
     * A process is exec()'ed automatically by the constructor.
     * You may manually exec() it again at any time to reset it.
     *
     * (This function does not start the process, only prepares it.)
     */
    void exec(const char *cmdLine);

    /*
     * The SBTHardware instance used by this process to emulate
     * software interrupts and I/O operations.
     *
     * Must be set once before the first run(), but you may also set
     * it before any subsequent run().
     */
    SBTHardware *hardware;

    /*
     * Run this process until the current entry point returns.
     */
    void run(void);

    /*
     * Interrupt the process.
     * It's an error to run() again without resetting via exec().
     */
    void exit(void);

    /*
     * Yield execution now (exiting all nested functions) and
     * resume at fn() next time run() is called. If default_entry
     * is true, this also becomes the fn() we run after the last fn() returns.
     */
    typedef void (*continue_func_t)(void);
    void continueFrom(SBTRegs regs, continue_func_t fn, bool default_entry = false);

    SBTRegs reg;
    SBTRegs default_reg;
    jmp_buf jmp_yield;
    continue_func_t continue_func;
    continue_func_t default_func;

    void failedDynamicBranch(uint16_t cs, uint16_t ip, uint32_t value);

    virtual uint16_t getAddress(SBTAddressId id) = 0;
    virtual const char *getFilename() = 0;

 protected:
    /*
     * Virtual functions generated by SBT86
     */
    virtual void loadCache(SBTStack *stack) = 0;
    virtual void saveCache() = 0;
    virtual uint8_t *getData() = 0;
    virtual uint32_t getDataLen() = 0;
    virtual uint16_t getRelocSegment() = 0;
    virtual uint16_t getEntryCS() = 0;
    virtual continue_func_t getEntry() = 0;
};


/*
 * SBTSegmentCache --
 *
 *    Cached segment pointers. Translated indirects use these pointers
 *    to avoid segment lookups on every memory access.
 */

class SBTSegmentCache
{
 public:
    uint8_t *cs;
    uint8_t *ds;
    uint8_t *es;
    uint8_t *ss;

    /*
     * Functions to load each cached segment.
     */
    SBT_INLINE void loadCS(SBTProcess *process, SBTRegs reg) {
        cs = process->hardware->memSeg(reg.cs);
    }
    SBT_INLINE void loadDS(SBTProcess *process, SBTRegs reg) {
        ds = process->hardware->memSeg(reg.ds);
    }
    SBT_INLINE void loadES(SBTProcess *process, SBTRegs reg) {
        es = process->hardware->memSeg(reg.es);
    }
    SBT_INLINE void loadSS(SBTProcess *process, SBTRegs reg) {
        ss = process->hardware->memSeg(reg.ss);
    }

    SBT_INLINE void load(SBTProcess *process, SBTRegs reg) {
        loadCS(process, reg);
        loadDS(process, reg);
        loadES(process, reg);
        loadSS(process, reg);
    }

    /*
     * Static utilities for 16-bit reads and writes. We have to split
     * this up into two 8-bit reads/writes, to avoid the ARM's
     * alignment constraints.
     */
    static SBT_INLINE uint16_t read16(uint8_t *ptr) {
        return ptr[0] | (ptr[1] << 8);
    }
    static SBT_INLINE void write16(uint8_t *ptr, uint16_t x) {
        ptr[0] = (uint8_t) x;
        ptr[1] = x >> 8;
    }
};


/*
 * SBT_DECL_PROCESS --
 *
 *    A macro to declare a process subclass created by SBT86.
 *    This is basically the replacement for a header files for
 *    these classes.
 */

#define SBT_DECL_PROCESS(name)                          \
    class name : public SBTProcess                      \
    {                                                   \
    public:                                             \
        name(SBTHardware *hardware,                     \
             const char *cmdLine = "") {                \
            this->hardware = hardware;                  \
            exec(cmdLine);                              \
        }                                               \
    protected:                                          \
        virtual void loadCache(SBTStack *stack);        \
        virtual void saveCache();                       \
        virtual uint8_t *getData();                     \
        virtual uint32_t getDataLen();                  \
        virtual uint16_t getRelocSegment();             \
        virtual uint16_t getEntryCS();                  \
        virtual continue_func_t getEntry();             \
    public:                                             \
        virtual uint16_t getAddress(SBTAddressId id);   \
        virtual const char *getFilename();              \
    }

