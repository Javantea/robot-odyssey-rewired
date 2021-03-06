import base64 from 'base64-arraybuffer';

var last_set_window_hash = null;
var autosave_timer = null;
const autosave_delay = 500;

function doAutoSave(engine)
{
    // this is hacky... autosaves have a different
    // completion action, and they shouldn't clobber
    // the user's save buffer (it's especially annoying
    // when loading chips), so I'm saving and restoring
    // the callback and contents, for now. This could
    // be improved by using a modified filename for the
    // autosave, or otherwise asking the engine to use
    // a different buffer than usual.
    // fix me!

    const saved_contents = engine.getSaveFile().slice();
    const saved_callback = engine.onSaveFileWrite;
    engine.onSaveFileWrite = function () {};

    try {
        var hash = '';

        switch (engine.saveGame()) {

            case engine.SaveStatus.OK:
                hash = base64.encode(engine.packSaveFile());
                console.log(`autoSave, ${hash.length} bytes`);
                break;

            case engine.SaveStatus.BLOCKED:
                return;

            case engine.SaveStatus.NOT_SUPPORTED:
                hash = '';
                break;
        }

        last_set_window_hash = hash;
        window.location.hash = hash;

    } finally {

        engine.onSaveFileWrite = saved_callback;
        engine.setSaveFile(saved_contents);
    }
}

function checkHashForAutoSave(engine)
{
    const hash = window.location.hash;
    if (hash && hash[0] == "#") {
        var s = hash.slice(1);
        if (s != last_set_window_hash) {
            const packed = new Uint8Array(base64.decode(s));
            if (engine.unpackSaveFile(packed)) {
                if (engine.loadGame()) {
                    console.log("Loading packed saved game");
                } else {
                    console.warn("FAILED to load packed saved game");
                }
            } else {
                console.warn("FAILED to unpack saved game");
            }
        }
    }
}

export function initAutoSave(engine)
{
    engine.autoSave = function ()
    {
        if (autosave_timer) {
            clearTimeout(autosave_timer);
        }
        autosave_timer = setTimeout(function () {
            doAutoSave(engine);
        }, autosave_delay);
    };

    window.addEventListener('hashchange', function () {
        checkHashForAutoSave(engine);
    });

    checkHashForAutoSave(engine);
}
