"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.extractSignatureMaterial = void 0;
function extractSignatureMaterial(dsseEnvelope, publicKey) {
    const signature = dsseEnvelope.signatures[0];
    return {
        signature: signature.sig,
        key: {
            id: signature.keyid,
            value: publicKey,
        },
        certificates: undefined,
    };
}
exports.extractSignatureMaterial = extractSignatureMaterial;
