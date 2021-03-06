export function loadingBegin()
{
    document.getElementById("engine_loading").style.display = "block";
}

export function loadingError(err)
{
    document.getElementById("engine_loading").style.display = "none";
    const el = document.getElementById("engine_error");

    err = err.toString();
    if (err.includes("no binaryen method succeeded")) {
        // This is obtuse; we only build for wasm, so really this means the device doesn't support wasm
        err = "No WebAssembly?\n\nSorry, this browser might not be supported.";
    } else {
        // Something else went wrong.
        err = "Fail.\n\n" + err;
    }

    el.innerText = err;
    el.style.display = "block";
}

export function loadingDone()
{
    document.getElementById("engine_loading").style.display = "none";
    document.getElementById("controls").className = "controls_visible";
}
