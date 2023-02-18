"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.toProposedIntotoEntry = exports.toProposedHashedRekordEntry = void 0;
const util_1 = require("../util");
const types_1 = require("./types");
const DEFAULT_HASHEDREKORD_API_VERSION = '0.0.1';
const DEFAULT_INTOTO_API_VERSION = '0.0.2';
// Returns a properly formatted Rekor "hashedrekord" entry for the given digest
// and signature
function toProposedHashedRekordEntry(digest, signature) {
    const hexDigest = digest.toString('hex');
    const b64Signature = signature.signature.toString('base64');
    const b64Key = util_1.encoding.base64Encode(toPublicKey(signature));
    return {
        apiVersion: DEFAULT_HASHEDREKORD_API_VERSION,
        kind: types_1.HASHEDREKORD_KIND,
        spec: {
            data: {
                hash: {
                    algorithm: 'sha256',
                    value: hexDigest,
                },
            },
            signature: {
                content: b64Signature,
                publicKey: {
                    content: b64Key,
                },
            },
        },
    };
}
exports.toProposedHashedRekordEntry = toProposedHashedRekordEntry;
// Returns a properly formatted Rekor "intoto" entry for the given DSSE
// envelope and signature
function toProposedIntotoEntry(envelope, signature, apiVersion = DEFAULT_INTOTO_API_VERSION) {
    switch (apiVersion) {
        case '0.0.2':
            return toProposedIntotoV002Entry(envelope, signature);
        default:
            throw new Error(`Unsupported intoto kind API version: ${apiVersion}`);
    }
}
exports.toProposedIntotoEntry = toProposedIntotoEntry;
function toProposedIntotoV002Entry(envelope, signature) {
    // Calculate the value for the payloadHash field in the Rekor entry
    const payloadHash = util_1.crypto.hash(envelope.payload).toString('hex');
    // Calculate the value for the hash field in the Rekor entry
    const envelopeHash = calculateDSSEHash(envelope);
    // Collect values for re-creating the DSSE envelope.
    // Double-encode payload and signature cause that's what Rekor expects
    const payload = util_1.encoding.base64Encode(envelope.payload.toString('base64'));
    const sig = util_1.encoding.base64Encode(envelope.signatures[0].sig.toString('base64'));
    const keyid = envelope.signatures[0].keyid;
    const publicKey = util_1.encoding.base64Encode(toPublicKey(signature));
    // Create the envelope portion of the entry. Note the inclusion of the
    // publicKey in the signature struct is not a standard part of a DSSE
    // envelope, but is required by Rekor.
    const dsse = {
        payloadType: envelope.payloadType,
        payload: payload,
        signatures: [{ sig, publicKey }],
    };
    // If the keyid is an empty string, Rekor seems to remove it altogether. We
    // need to do the same here so that we can properly recreate the entry for
    // verification.
    if (keyid.length > 0) {
        dsse.signatures[0].keyid = keyid;
    }
    return {
        apiVersion: '0.0.2',
        kind: types_1.INTOTO_KIND,
        spec: {
            content: {
                envelope: dsse,
                hash: { algorithm: 'sha256', value: envelopeHash },
                payloadHash: { algorithm: 'sha256', value: payloadHash },
            },
        },
    };
}
// Calculates the hash of a DSSE envelope for inclusion in a Rekor entry.
// There is no standard way to do this, so the scheme we're using as as
// follows:
//  * payload is base64 encoded
//  * signature is base64 encoded (only the first signature is used)
//  * keyid is included ONLY if it is NOT an empty string
//  * The resulting JSON is canonicalized and hashed to a hex string
function calculateDSSEHash(envelope) {
    const dsse = {
        payloadType: envelope.payloadType,
        payload: envelope.payload.toString('base64'),
        signatures: [{ sig: envelope.signatures[0].sig.toString('base64') }],
    };
    // If the keyid is an empty string, Rekor seems to remove it altogether.
    if (envelope.signatures[0].keyid.length > 0) {
        dsse.signatures[0].keyid = envelope.signatures[0].keyid;
    }
    return util_1.crypto.hash(util_1.json.canonicalize(dsse)).toString('hex');
}
function toPublicKey(signature) {
    return signature.certificates
        ? signature.certificates[0]
        : signature.key.value;
}
