"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MinipassSized = exports.SizeError = void 0;
const minipass_1 = require("minipass");
const isBufferEncoding = (enc) => typeof enc === 'string';
class SizeError extends Error {
    expect;
    found;
    code = 'EBADSIZE';
    constructor(found, expect, from) {
        super(`Bad data size: expected ${expect} bytes, but got ${found}`);
        this.expect = expect;
        this.found = found;
        Error.captureStackTrace(this, from ?? this.constructor);
    }
    get name() {
        return 'SizeError';
    }
}
exports.SizeError = SizeError;
class MinipassSized extends minipass_1.Minipass {
    found = 0;
    expect;
    constructor(options) {
        const size = options?.size;
        if (typeof size !== 'number' ||
            size > Number.MAX_SAFE_INTEGER ||
            isNaN(size) ||
            size < 0 ||
            !isFinite(size) ||
            size !== Math.floor(size)) {
            throw new Error('invalid expected size: ' + size);
        }
        //@ts-ignore
        super(options);
        if (options.objectMode) {
            throw new TypeError(`${this.constructor.name} streams only work with string and buffer data`);
        }
        this.expect = size;
    }
    write(chunk, encoding, cb) {
        const buffer = Buffer.isBuffer(chunk) ? chunk
            : typeof chunk === 'string' ?
                Buffer.from(chunk, isBufferEncoding(encoding) ? encoding : 'utf8')
                : chunk;
        if (typeof encoding === 'function') {
            cb = encoding;
            encoding = null;
        }
        if (!Buffer.isBuffer(buffer)) {
            this.emit('error', new TypeError(`${this.constructor.name} streams only work with string and buffer data`));
            return false;
        }
        this.found += buffer.length;
        if (this.found > this.expect)
            this.emit('error', new SizeError(this.found, this.expect));
        return super.write(chunk, encoding, cb);
    }
    emit(ev, ...args) {
        if (ev === 'end') {
            if (this.found !== this.expect) {
                this.emit('error', new SizeError(this.found, this.expect, this.emit));
            }
        }
        return super.emit(ev, ...args);
    }
}
exports.MinipassSized = MinipassSized;
//# sourceMappingURL=index.js.map