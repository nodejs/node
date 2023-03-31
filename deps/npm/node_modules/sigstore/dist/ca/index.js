"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.CAClient = void 0;
const client_1 = require("../client");
const error_1 = require("../error");
const format_1 = require("./format");
class CAClient {
    constructor(options) {
        this.fulcio = new client_1.Fulcio({ baseURL: options.fulcioBaseURL });
    }
    async createSigningCertificate(identityToken, publicKey, challenge) {
        const request = (0, format_1.toCertificateRequest)(identityToken, publicKey, challenge);
        try {
            const certificate = await this.fulcio.createSigningCertificate(request);
            return certificate.signedCertificateEmbeddedSct.chain.certificates;
        }
        catch (err) {
            throw new error_1.InternalError('error creating signing certificate', err);
        }
    }
}
exports.CAClient = CAClient;
