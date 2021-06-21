"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.createLogSubject = void 0;
const child_process_1 = require("child_process");
const rxjs_1 = require("rxjs");
const path_1 = __importDefault(require("path"));
const types_1 = require("./types");
// max-api is not imported from node_modules, but from Node4Max runtime
// @ts-ignore - no typescript typings yet
const max_api_1 = require("max-api");
const handles = new Map();
const loops = new Map();
const spawnNode = (...args) => {
    // must compile node before running
    const nodePath = path_1.default.join(__dirname, '../../out/Release/node');
    return child_process_1.spawn(nodePath, args);
};
const parseLogData = (logLn) => {
    const [name, ...data] = logLn.split(' ');
    switch (name.trim()) {
        case 'uv__handle_init':
            const [handleAddress, loopAddress, typeStr] = data;
            const type = Number.parseInt(typeStr);
            const handle = {
                type,
                typeName: types_1.UVHandleTypeNames[type],
                address: handleAddress,
                loop: loopAddress
            };
            handles.set(handleAddress, handle);
            return ['handle_init', handle];
        case 'uv__handle_start':
            const [startAddress] = data;
            return ['handle_start', handles.get(startAddress)];
        case 'uv__handle_stop':
            const [stopAddress] = data;
            return ['handle_stop', handles.get(stopAddress)];
        case 'uv_run':
            const [loopRunAddress, , modeStr] = data;
            const mode = Number.parseInt(modeStr);
            const loop = {
                type: mode + types_1.UVHandleTypeNames.length,
                typeName: types_1.UVRunModeName[mode],
                address: loopRunAddress
            };
            loops.set(loopRunAddress, loop);
            return ['loop_run', loop];
        case 'uv__loop_alive':
            const [loop_alive_address] = data;
            return ['loop_alive', loops.get(loop_alive_address)];
    }
};
const createLogSubject = (proc) => {
    const log$ = new rxjs_1.Subject();
    proc.on('data', (chunk) => chunk.toString().split('\n').forEach((str) => log$.next(str)));
    proc.stdout.on('data', (chunk) => chunk.toString().split('\n').forEach((str) => log$.next(str)));
    proc.stdout.on('error', (err) => log$.error(err));
    proc.on('error', (err) => log$.error(err));
    proc.on('close', () => log$.complete());
    proc.on('exit', () => log$.complete());
    return log$;
};
exports.createLogSubject = createLogSubject;
try {
    // retrieve the third argument through destructuring
    // arguments are passed from a Max message
    const [, , scriptFile] = process.argv;
    const nodeProcess = spawnNode(scriptFile);
    exports.createLogSubject(nodeProcess).subscribe((log) => {
        var _a;
        const [state, data] = (_a = parseLogData(log)) !== null && _a !== void 0 ? _a : [];
        if (!data)
            return;
        const note = data.type + 100; // adds 100 for higher MIDI note range
        max_api_1.post(state, data.typeName, note);
        switch (state) {
            case 'handle_init':
                max_api_1.outlet([note, 50]);
                break;
            case 'handle_start':
                max_api_1.outlet([note, 100]);
                break;
            case 'handle_stop':
                max_api_1.outlet([note, 0]);
                break;
            case 'loop_run':
                max_api_1.outlet([note, 100]);
                break;
            case 'loop_alive':
                max_api_1.outlet([note, 50]);
                break;
        }
    });
}
catch (err) {
    process.exit(1);
}
