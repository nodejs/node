"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.TUFError = void 0;
class TUFError extends Error {
    code;
    cause;
    constructor({ code, message, cause, }) {
        super(message);
        this.code = code;
        this.cause = cause;
        this.name = this.constructor.name;
    }
}
exports.TUFError = TUFError;
