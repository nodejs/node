"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.CAClient = void 0;
const error_1 = require("../error");
const external_1 = require("../external");
const format_1 = require("./format");
class CAClient {
    constructor(options) {
        this.fulcio = new external_1.Fulcio({
            baseURL: options.fulcioBaseURL,
            retry: options.retry,
            timeout: options.timeout,
        });
    }
    async createSigningCertificate(identityToken, publicKey, challenge) {
        const request = (0, format_1.toCertificateRequest)(identityToken, publicKey, challenge);
        try {
            const resp = await this.fulcio.createSigningCertificate(request);
            // Account for the fact that the response may contain either a
            // signedCertificateEmbeddedSct or a signedCertificateDetachedSct.
            const cert = resp.signedCertificateEmbeddedSct
                ? resp.signedCertificateEmbeddedSct
                : resp.signedCertificateDetachedSct;
            // Return the first certificate in the chain, which is the signing
            // certificate. Specifically not returning the rest of the chain to
            // mitigate the risk of errors when verifying the certificate chain.
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            return cert.chain.certificates.slice(0, 1);
        }
        catch (err) {
            throw new error_1.InternalError({
                code: 'CA_CREATE_SIGNING_CERTIFICATE_ERROR',
                message: 'error creating signing certificate',
                cause: err,
            });
        }
    }
}
exports.CAClient = CAClient;
