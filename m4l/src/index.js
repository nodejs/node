"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.createLogger = void 0;
const child_process_1 = require("child_process");
const rxjs_1 = require("rxjs");
const path_1 = __importDefault(require("path"));
// @ts-ignore
// import { post } from 'max-api';
const post = console.log;
var uv_handle_type;
(function (uv_handle_type) {
    uv_handle_type[uv_handle_type["UV_UNKNOWN_HANDLE"] = 0] = "UV_UNKNOWN_HANDLE";
    uv_handle_type[uv_handle_type["UV_ASYNC"] = 1] = "UV_ASYNC";
    uv_handle_type[uv_handle_type["UV_CHECK"] = 2] = "UV_CHECK";
    uv_handle_type[uv_handle_type["UV_FS_EVENT"] = 3] = "UV_FS_EVENT";
    uv_handle_type[uv_handle_type["UV_FS_POLL"] = 4] = "UV_FS_POLL";
    uv_handle_type[uv_handle_type["UV_HANDLE"] = 5] = "UV_HANDLE";
    uv_handle_type[uv_handle_type["UV_IDLE"] = 6] = "UV_IDLE";
    uv_handle_type[uv_handle_type["UV_NAMED_PIPE"] = 7] = "UV_NAMED_PIPE";
    uv_handle_type[uv_handle_type["UV_POLL"] = 8] = "UV_POLL";
    uv_handle_type[uv_handle_type["UV_PREPARE"] = 9] = "UV_PREPARE";
    uv_handle_type[uv_handle_type["UV_PROCESS"] = 10] = "UV_PROCESS";
    uv_handle_type[uv_handle_type["UV_STREAM"] = 11] = "UV_STREAM";
    uv_handle_type[uv_handle_type["UV_TCP"] = 12] = "UV_TCP";
    uv_handle_type[uv_handle_type["UV_TIMER"] = 13] = "UV_TIMER";
    uv_handle_type[uv_handle_type["UV_TTY"] = 14] = "UV_TTY";
    uv_handle_type[uv_handle_type["UV_UDP"] = 15] = "UV_UDP";
    uv_handle_type[uv_handle_type["UV_SIGNAL"] = 16] = "UV_SIGNAL";
    uv_handle_type[uv_handle_type["UV_FILE"] = 17] = "UV_FILE";
    uv_handle_type[uv_handle_type["UV_HANDLE_TYPE_MAX"] = 18] = "UV_HANDLE_TYPE_MAX";
})(uv_handle_type || (uv_handle_type = {}));
var uv_run_mode;
(function (uv_run_mode) {
    uv_run_mode[uv_run_mode["UV_RUN_DEFAULT"] = 0] = "UV_RUN_DEFAULT";
    uv_run_mode[uv_run_mode["UV_RUN_ONCE"] = 1] = "UV_RUN_ONCE";
    uv_run_mode[uv_run_mode["UV_RUN_NOWAIT"] = 2] = "UV_RUN_NOWAIT";
})(uv_run_mode || (uv_run_mode = {}));
var State;
(function (State) {
    State[State["init"] = 0] = "init";
    State[State["start"] = 1] = "start";
    State[State["stop"] = 2] = "stop";
    State[State["run"] = 3] = "run";
    State[State["alive"] = 4] = "alive";
})(State || (State = {}));
const handles = new Map();
const loops = new Map();
const spawnNode = (...args) => {
    const nodePath = path_1.default.join(__dirname, '../../out/Release/node');
    post(`spawning node at ${nodePath}`);
    return child_process_1.spawn(nodePath, args);
};
const parseData = (logLn) => {
    post(`parsing ${logLn}`);
    const [name, ...data] = logLn.split(' ');
    switch (name.trim()) {
        case 'uv__handle_init':
            const [handle_address, loop_address, type] = data;
            const handle = {
                type: Number.parseInt(type),
                address: handle_address,
                loop: loop_address
            };
            handles.set(handle_address, handle);
            return [State.init, handle];
        case 'uv__handle_start':
            const [start_address] = data;
            return [State.start, handles.get(start_address)];
        case 'uv__handle_stop':
            const [stop_address] = data;
            return [State.stop, handles.get(stop_address)];
        case 'uv_run':
            const [loop_run_address] = data;
            const loop = {
                address: loop_run_address
            };
            loops.set(loop_run_address, loop);
            return [State.run, loop];
        case 'uv__loop_alive':
            const [loop_alive_address] = data;
            return [State.alive, loops.get(loop_alive_address)];
    }
};
const createLogger = (proc) => {
    post('creating logger');
    const logger$ = new rxjs_1.Subject();
    proc.on('data', (chunk) => logger$.next(chunk.toString()));
    proc.stdout.on('data', (chunk) => logger$.next(chunk.toString()));
    proc.stdout.on('error', (err) => logger$.error(err));
    proc.on('error', (err) => logger$.error(err));
    proc.on('close', () => logger$.complete());
    proc.on('exit', () => logger$.complete());
    return logger$;
};
exports.createLogger = createLogger;
try {
    post('will start');
    const proc = spawnNode('-e', '2+2');
    const logger$ = exports.createLogger(proc);
    logger$.subscribe((log) => {
        var _a;
        const [state, handle] = (_a = parseData(log)) !== null && _a !== void 0 ? _a : [];
        post(`${state} ${handle}`);
    });
}
catch (err) {
    post(`${err}`);
    process.exit(1);
}
