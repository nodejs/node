"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.appDataPath = void 0;
const os_1 = __importDefault(require("os"));
const path_1 = __importDefault(require("path"));
function appDataPath(name) {
    const homedir = os_1.default.homedir();
    switch (process.platform) {
        case 'darwin': {
            const appSupport = path_1.default.join(homedir, 'Library', 'Application Support');
            return path_1.default.join(appSupport, name);
        }
        case 'win32': {
            const localAppData = process.env.LOCALAPPDATA || path_1.default.join(homedir, 'AppData', 'Local');
            return path_1.default.join(localAppData, name, 'Data');
        }
        default: {
            const localData = process.env.XDG_DATA_HOME || path_1.default.join(homedir, '.local', 'share');
            return path_1.default.join(localData, name);
        }
    }
}
exports.appDataPath = appDataPath;
