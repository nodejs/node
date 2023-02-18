"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Signature = void 0;
/**
 * A container class containing information about a signature.
 *
 * Contains a signature and the keyid uniquely identifying the key used
 * to generate the signature.
 *
 * Provide a `fromJSON` method to create a Signature from a JSON object.
 */
class Signature {
    constructor(options) {
        const { keyID, sig } = options;
        this.keyID = keyID;
        this.sig = sig;
    }
    static fromJSON(data) {
        const { keyid, sig } = data;
        if (typeof keyid !== 'string') {
            throw new TypeError('keyid must be a string');
        }
        if (typeof sig !== 'string') {
            throw new TypeError('sig must be a string');
        }
        return new Signature({
            keyID: keyid,
            sig: sig,
        });
    }
}
exports.Signature = Signature;
