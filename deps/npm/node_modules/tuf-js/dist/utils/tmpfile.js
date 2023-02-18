"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.withTempFile = void 0;
const promises_1 = __importDefault(require("fs/promises"));
const os_1 = __importDefault(require("os"));
const path_1 = __importDefault(require("path"));
// Invokes the given handler with the path to a temporary file. The file
// is deleted after the handler returns.
const withTempFile = async (handler) => withTempDir(async (dir) => handler(path_1.default.join(dir, 'tempfile')));
exports.withTempFile = withTempFile;
// Invokes the given handler with a temporary directory. The directory is
// deleted after the handler returns.
const withTempDir = async (handler) => {
    const tmpDir = await promises_1.default.realpath(os_1.default.tmpdir());
    const dir = await promises_1.default.mkdtemp(tmpDir + path_1.default.sep);
    try {
        return await handler(dir);
    }
    finally {
        await promises_1.default.rm(dir, { force: true, recursive: true, maxRetries: 3 });
    }
};
