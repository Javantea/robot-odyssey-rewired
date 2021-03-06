// Some information about Robot Odyssey world data,
// implemented directly in Javascript. See the main
// API for world data on the C++ side, in roData.h

const LAB_WORLD = 30;

export function worldIdFromSaveData(bytes)
{
    if (bytes.length == 24389) {
        return bytes[bytes.length - 1];
    }
    return null;
}

export function chipNameFromSaveData(bytes)
{
    if (bytes.length == 1333) {
        var name = '';
        const nameField = bytes.slice(0x40A, 0x41C);
        for (let byte of nameField) {
            if (byte < 0x20 || byte > 0x7F) {
                break;
            }
            name += String.fromCharCode(byte);
        }
        return name.trim() || "untitled";
    }
    return null;
}

export function filenameForSaveData(bytes)
{
    const world = worldIdFromSaveData(bytes);
    const chip = chipNameFromSaveData(bytes);
    const now = new Date();

    if (world !== null) {
        if (world == LAB_WORLD) {
            return "robotodyssey-lab-" + now.toISOString() + ".lsv";
        } else {
            return "robotodyssey-world" + (world+1) + "-" + now.toISOString() + ".gsv";
        }
    }
    if (chip !== null) {
        return "robotodyssey-chip-" + chip + "-" + now.toISOString() + ".csv";
    }
    return "robotodyssey-" + now.toISOString() + ".bin";
}
