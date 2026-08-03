"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Key = void 0;
const util_1 = __importDefault(require("util"));
const error_1 = require("./error");
const utils_1 = require("./utils");
const key_1 = require("./utils/key");
// A container class representing the public portion of a Key.
class Key {
    keyID;
    keyType;
    scheme;
    keyVal;
    unrecognizedFields;
    constructor(options) {
        const { keyID, keyType, scheme, keyVal, unrecognizedFields } = options;
        this.keyID = keyID;
        this.keyType = keyType;
        this.scheme = scheme;
        this.keyVal = keyVal;
        this.unrecognizedFields = unrecognizedFields || {};
    }
    // Verifies the that the metadata.signatures contains a signature made with
    // this key and is correctly signed.
    verifySignature(metadata) {
        const signature = metadata.signatures[this.keyID];
        if (!signature)
            throw new error_1.UnsignedMetadataError('no signature for key found in metadata');
        if (!this.keyVal.public)
            throw new error_1.UnsignedMetadataError('no public key found');
        const publicKey = (0, key_1.getPublicKey)({
            keyType: this.keyType,
            scheme: this.scheme,
            keyVal: this.keyVal.public,
        });
        const signedData = metadata.signed.toJSON();
        try {
            if (!utils_1.crypto.verifySignature(signedData, publicKey, signature.sig)) {
                throw new error_1.UnsignedMetadataError(`failed to verify ${this.keyID} signature`);
            }
        }
        catch (error) {
            if (error instanceof error_1.UnsignedMetadataError) {
                throw error;
            }
            throw new error_1.UnsignedMetadataError(`failed to verify ${this.keyID} signature`);
        }
    }
    equals(other) {
        if (!(other instanceof Key)) {
            return false;
        }
        return (this.keyID === other.keyID &&
            this.keyType === other.keyType &&
            this.scheme === other.scheme &&
            util_1.default.isDeepStrictEqual(this.keyVal, other.keyVal) &&
            util_1.default.isDeepStrictEqual(this.unrecognizedFields, other.unrecognizedFields));
    }
    toJSON() {
        return {
            keytype: this.keyType,
            scheme: this.scheme,
            keyval: this.keyVal,
            ...this.unrecognizedFields,
        };
    }
    static fromJSON(keyID, data) {
        const { keytype, scheme, keyval, ...rest } = data;
        if (typeof keytype !== 'string') {
            throw new TypeError('keytype must be a string');
        }
        if (typeof scheme !== 'string') {
            throw new TypeError('scheme must be a string');
        }
        if (!utils_1.guard.isStringRecord(keyval)) {
            throw new TypeError('keyval must be a string record');
        }
        return new Key({
            keyID,
            keyType: keytype,
            scheme,
            keyVal: keyval,
            unrecognizedFields: rest,
        });
    }
}
exports.Key = Key;
