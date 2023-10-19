"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.TargetFile = exports.MetaFile = void 0;
const crypto_1 = __importDefault(require("crypto"));
const util_1 = __importDefault(require("util"));
const error_1 = require("./error");
const utils_1 = require("./utils");
// A container with information about a particular metadata file.
//
// This class is used for Timestamp and Snapshot metadata.
class MetaFile {
    constructor(opts) {
        if (opts.version <= 0) {
            throw new error_1.ValueError('Metafile version must be at least 1');
        }
        if (opts.length !== undefined) {
            validateLength(opts.length);
        }
        this.version = opts.version;
        this.length = opts.length;
        this.hashes = opts.hashes;
        this.unrecognizedFields = opts.unrecognizedFields || {};
    }
    equals(other) {
        if (!(other instanceof MetaFile)) {
            return false;
        }
        return (this.version === other.version &&
            this.length === other.length &&
            util_1.default.isDeepStrictEqual(this.hashes, other.hashes) &&
            util_1.default.isDeepStrictEqual(this.unrecognizedFields, other.unrecognizedFields));
    }
    verify(data) {
        // Verifies that the given data matches the expected length.
        if (this.length !== undefined) {
            if (data.length !== this.length) {
                throw new error_1.LengthOrHashMismatchError(`Expected length ${this.length} but got ${data.length}`);
            }
        }
        // Verifies that the given data matches the supplied hashes.
        if (this.hashes) {
            Object.entries(this.hashes).forEach(([key, value]) => {
                let hash;
                try {
                    hash = crypto_1.default.createHash(key);
                }
                catch (e) {
                    throw new error_1.LengthOrHashMismatchError(`Hash algorithm ${key} not supported`);
                }
                const observedHash = hash.update(data).digest('hex');
                if (observedHash !== value) {
                    throw new error_1.LengthOrHashMismatchError(`Expected hash ${value} but got ${observedHash}`);
                }
            });
        }
    }
    toJSON() {
        const json = {
            version: this.version,
            ...this.unrecognizedFields,
        };
        if (this.length !== undefined) {
            json.length = this.length;
        }
        if (this.hashes) {
            json.hashes = this.hashes;
        }
        return json;
    }
    static fromJSON(data) {
        const { version, length, hashes, ...rest } = data;
        if (typeof version !== 'number') {
            throw new TypeError('version must be a number');
        }
        if (utils_1.guard.isDefined(length) && typeof length !== 'number') {
            throw new TypeError('length must be a number');
        }
        if (utils_1.guard.isDefined(hashes) && !utils_1.guard.isStringRecord(hashes)) {
            throw new TypeError('hashes must be string keys and values');
        }
        return new MetaFile({
            version,
            length,
            hashes,
            unrecognizedFields: rest,
        });
    }
}
exports.MetaFile = MetaFile;
// Container for info about a particular target file.
//
// This class is used for Target metadata.
class TargetFile {
    constructor(opts) {
        validateLength(opts.length);
        this.length = opts.length;
        this.path = opts.path;
        this.hashes = opts.hashes;
        this.unrecognizedFields = opts.unrecognizedFields || {};
    }
    get custom() {
        const custom = this.unrecognizedFields['custom'];
        if (!custom || Array.isArray(custom) || !(typeof custom === 'object')) {
            return {};
        }
        return custom;
    }
    equals(other) {
        if (!(other instanceof TargetFile)) {
            return false;
        }
        return (this.length === other.length &&
            this.path === other.path &&
            util_1.default.isDeepStrictEqual(this.hashes, other.hashes) &&
            util_1.default.isDeepStrictEqual(this.unrecognizedFields, other.unrecognizedFields));
    }
    async verify(stream) {
        let observedLength = 0;
        // Create a digest for each hash algorithm
        const digests = Object.keys(this.hashes).reduce((acc, key) => {
            try {
                acc[key] = crypto_1.default.createHash(key);
            }
            catch (e) {
                throw new error_1.LengthOrHashMismatchError(`Hash algorithm ${key} not supported`);
            }
            return acc;
        }, {});
        // Read stream chunk by chunk
        for await (const chunk of stream) {
            // Keep running tally of stream length
            observedLength += chunk.length;
            // Append chunk to each digest
            Object.values(digests).forEach((digest) => {
                digest.update(chunk);
            });
        }
        // Verify length matches expected value
        if (observedLength !== this.length) {
            throw new error_1.LengthOrHashMismatchError(`Expected length ${this.length} but got ${observedLength}`);
        }
        // Verify each digest matches expected value
        Object.entries(digests).forEach(([key, value]) => {
            const expected = this.hashes[key];
            const actual = value.digest('hex');
            if (actual !== expected) {
                throw new error_1.LengthOrHashMismatchError(`Expected hash ${expected} but got ${actual}`);
            }
        });
    }
    toJSON() {
        return {
            length: this.length,
            hashes: this.hashes,
            ...this.unrecognizedFields,
        };
    }
    static fromJSON(path, data) {
        const { length, hashes, ...rest } = data;
        if (typeof length !== 'number') {
            throw new TypeError('length must be a number');
        }
        if (!utils_1.guard.isStringRecord(hashes)) {
            throw new TypeError('hashes must have string keys and values');
        }
        return new TargetFile({
            length,
            path,
            hashes,
            unrecognizedFields: rest,
        });
    }
}
exports.TargetFile = TargetFile;
// Check that supplied length if valid
function validateLength(length) {
    if (length < 0) {
        throw new error_1.ValueError('Length must be at least 0');
    }
}
