"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.toCertificateRequest = void 0;
function toCertificateRequest(identityToken, publicKey, challenge) {
    return {
        credentials: {
            oidcIdentityToken: identityToken,
        },
        publicKeyRequest: {
            publicKey: {
                algorithm: 'ECDSA',
                content: publicKey
                    .export({ format: 'pem', type: 'spki' })
                    .toString('ascii'),
            },
            proofOfPossession: challenge.toString('base64'),
        },
    };
}
exports.toCertificateRequest = toCertificateRequest;
