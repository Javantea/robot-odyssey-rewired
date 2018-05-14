import nipplejs from 'nipplejs';
import { audioContextSetup } from "./sound.js"

var joystick = null;

function controlCode(key)
{
    return String.fromCharCode(key.toUpperCase().charCodeAt(0) - 'A'.charCodeAt(0) + 1)
}

export function initInputEarly()
{
    // Joystick is created immediately, but callbacks aren't hooked up until the engine loads
    joystick = nipplejs.create({
        zone: document.getElementById('joystick_xy'),
        mode: 'static',
        size: '140',
        threshold: 0.001,
        position: { left: '50%', top: '50%' }
    });

    // Make it easy to hide the joystick on desktop
    document.getElementById('joystick_hide').addEventListener('click', (e) => {
        document.getElementById('joystick').style.display = 'none';
    });
}

export function initInputAfterEngineLoads(engine)
{
    const fbCanvas = document.getElementById('framebuffer');
    const frame = document.getElementById('frame');

    const canvas_width = fbCanvas.width;
    const canvas_height = fbCanvas.height;

    frame.addEventListener('mousemove', function (e)
    {
        const canvasRect = fbCanvas.getBoundingClientRect();

        const canvasX = (e.clientX - canvasRect.x) * canvas_width / canvasRect.width;
        const canvasY = (e.clientY - canvasRect.y) * canvas_height / canvasRect.height;
        const framebufferX = canvasX - 1;
        const framebufferY = canvasY - 1;

        const gameX = framebufferX / 2 - 2;
        const gameY = 188 - framebufferY;

        engine.setMouseTracking(gameX, gameY);
    });

    frame.addEventListener('mousedown', function (e)
    {
        if (e.button == 0) {
            e.preventDefault();
            engine.setMouseButton(true);
            audioContextSetup();
        }
    });

    frame.addEventListener('mouseup', function (e)
    {
        engine.setMouseButton(false);
        engine.autoSave();
        audioContextSetup();
    });

    frame.addEventListener('mouseout', function (e)
    {
        engine.endMouseTracking();
    });

    function refocus(e)
    {
        e.target.blur();
        fbCanvas.focus();
    }

    function keycode(ascii, scancode)
    {
        if (typeof(ascii) != typeof(0)) {
            ascii = ascii.length == 1 ? ascii.charCodeAt(0) : parseInt(ascii, 0);
        }
        engine.pressKey(ascii, scancode);
        engine.endMouseTracking();
        engine.autoSave();
        audioContextSetup();
    }

    document.body.addEventListener('keydown', function (e)
    {
        if (e.code == "ArrowUp" && e.shiftKey == false) keycode(0, 0x48);
        else if (e.code == "ArrowUp" && e.shiftKey == true) keycode('8', 0x48);
        else if (e.code == "ArrowDown" && e.shiftKey == false) keycode(0, 0x50);
        else if (e.code == "ArrowDown" && e.shiftKey == true) keycode('2', 0x50);
        else if (e.code == "ArrowLeft" && e.shiftKey == false) keycode(0, 0x4B);
        else if (e.code == "ArrowLeft" && e.shiftKey == true) keycode('4', 0x4B);
        else if (e.code == "ArrowRight" && e.shiftKey == false) keycode(0, 0x4D);
        else if (e.code == "ArrowRight" && e.shiftKey == true) keycode('6', 0x4D);
        else if (e.code == "Backspace" && !e.ctrlKey && !e.altKey && !e.metaKey) keycode('\x08', 0);
        else if (e.code == "Enter" && !e.ctrlKey && !e.altKey && !e.metaKey) keycode('\x0D', 0x1C);
        else if (e.code == "Escape" && !e.ctrlKey && !e.altKey && !e.metaKey) keycode('\x1b', 0x01);
        else if (e.key.length == 1 && !e.ctrlKey && !e.altKey && !e.metaKey) keycode(e.key.toUpperCase(), 0);
        else if (e.key.length == 1 && e.ctrlKey && !e.altKey && !e.metaKey) keycode(controlCode(e.key), 0);
        else return;
        e.preventDefault();
    });

    joystick.on('move', function (e, data)
    {
        const scale = 8.0;
        const force = Math.min(10, Math.pow(data.force, 2));
        const x = scale * force * Math.cos(data.angle.radian);
        const y = scale * force * Math.sin(data.angle.radian);
        engine.endMouseTracking();
        engine.setJoystickAxes(x, -y);
    });

    joystick.on('end', function (e)
    {
        engine.setJoystickAxes(0, 0);
        engine.autoSave();
        audioContextSetup();
    });

    for (let button of document.getElementsByClassName('joystick_btn')) {
        button.addEventListener('mousedown', (e) => {
            engine.setJoystickButton(true);
            audioContextSetup();
        });
        button.addEventListener('mouseup', (e) => {
            engine.setJoystickButton(false);
            engine.autoSave();
            audioContextSetup();
            refocus(e);
        });
        button.addEventListener('touchstart', (e) => {
            e.preventDefault();
            engine.setJoystickButton(true);
            audioContextSetup();
            refocus(e);
        });
        button.addEventListener('touchend', (e) => {
            e.preventDefault();
            engine.setJoystickButton(false);
            engine.autoSave();
            audioContextSetup();
            refocus(e);
        });
    }

    for (let button of document.getElementsByClassName('exec_btn')) {
        button.addEventListener('click', (e) => {
            engine.exec(button.dataset.program, button.dataset.args);
            engine.autoSave();
            audioContextSetup();
            refocus(e);
        });
    }

    for (let button of document.getElementsByClassName('keyboard_btn')) {
        button.addEventListener('click', (e) => {
            keycode(button.dataset.ascii, parseInt(button.dataset.scancode, 0));
            engine.endMouseTracking();
            engine.autoSave();
            audioContextSetup();
            refocus(e);
        });
        button.addEventListener('touchend', (e) => {
            e.preventDefault();
            keycode(button.dataset.ascii, parseInt(button.dataset.scancode, 0));
            engine.autoSave();
            audioContextSetup();
            refocus(e);
        });
    }

    for (let button of document.getElementsByClassName('setspeed_btn')) {
        button.addEventListener('click', (e) => {
            engine.setSpeed(parseFloat(button.dataset.speed));
            audioContextSetup();
            refocus(e);
        });
    }
}