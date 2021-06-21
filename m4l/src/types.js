"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.UVRunMode = exports.UVRunModeName = exports.UVHandleType = exports.UVHandleTypeNames = void 0;
exports.UVHandleTypeNames = [
    'UV_UNKNOWN_HANDLE',
    'UV_ASYNC',
    'UV_CHECK',
    'UV_FS_EVENT',
    'UV_FS_POLL',
    'UV_HANDLE',
    'UV_IDLE',
    'UV_NAMED_PIPE',
    'UV_POLL',
    'UV_PREPARE',
    'UV_PROCESS',
    'UV_STREAM',
    'UV_TCP',
    'UV_TIMER',
    'UV_TTY',
    'UV_UDP',
    'UV_SIGNAL',
    'UV_FILE',
    'UV_HANDLE_TYPE_MAX'
];
var UVHandleType;
(function (UVHandleType) {
    UVHandleType[UVHandleType["UV_UNKNOWN_HANDLE"] = 0] = "UV_UNKNOWN_HANDLE";
    UVHandleType[UVHandleType["UV_ASYNC"] = 1] = "UV_ASYNC";
    UVHandleType[UVHandleType["UV_CHECK"] = 2] = "UV_CHECK";
    UVHandleType[UVHandleType["UV_FS_EVENT"] = 3] = "UV_FS_EVENT";
    UVHandleType[UVHandleType["UV_FS_POLL"] = 4] = "UV_FS_POLL";
    UVHandleType[UVHandleType["UV_HANDLE"] = 5] = "UV_HANDLE";
    UVHandleType[UVHandleType["UV_IDLE"] = 6] = "UV_IDLE";
    UVHandleType[UVHandleType["UV_NAMED_PIPE"] = 7] = "UV_NAMED_PIPE";
    UVHandleType[UVHandleType["UV_POLL"] = 8] = "UV_POLL";
    UVHandleType[UVHandleType["UV_PREPARE"] = 9] = "UV_PREPARE";
    UVHandleType[UVHandleType["UV_PROCESS"] = 10] = "UV_PROCESS";
    UVHandleType[UVHandleType["UV_STREAM"] = 11] = "UV_STREAM";
    UVHandleType[UVHandleType["UV_TCP"] = 12] = "UV_TCP";
    UVHandleType[UVHandleType["UV_TIMER"] = 13] = "UV_TIMER";
    UVHandleType[UVHandleType["UV_TTY"] = 14] = "UV_TTY";
    UVHandleType[UVHandleType["UV_UDP"] = 15] = "UV_UDP";
    UVHandleType[UVHandleType["UV_SIGNAL"] = 16] = "UV_SIGNAL";
    UVHandleType[UVHandleType["UV_FILE"] = 17] = "UV_FILE";
    UVHandleType[UVHandleType["UV_HANDLE_TYPE_MAX"] = 18] = "UV_HANDLE_TYPE_MAX";
})(UVHandleType = exports.UVHandleType || (exports.UVHandleType = {}));
exports.UVRunModeName = [
    'UV_RUN_DEFAULT',
    'UV_RUN_ONCE',
    'UV_RUN_NOWAIT'
];
var UVRunMode;
(function (UVRunMode) {
    UVRunMode[UVRunMode["UV_RUN_DEFAULT"] = 19] = "UV_RUN_DEFAULT";
    UVRunMode[UVRunMode["UV_RUN_ONCE"] = 20] = "UV_RUN_ONCE";
    UVRunMode[UVRunMode["UV_RUN_NOWAIT"] = 21] = "UV_RUN_NOWAIT";
})(UVRunMode = exports.UVRunMode || (exports.UVRunMode = {}));
