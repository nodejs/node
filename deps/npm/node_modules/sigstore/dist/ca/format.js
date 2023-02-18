"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.toCertificateRequest = void 0;
function toCertificateRequest(publicKey, challenge) {
    return {
        publicKey: {
            content: publicKey
                .export({ type: 'spki', format: 'der' })
                .toString('base64'),
        },
        signedEmailAddress: challenge.toString('base64'),
    };
}
exports.toCertificateRequest = toCertificateRequest;
