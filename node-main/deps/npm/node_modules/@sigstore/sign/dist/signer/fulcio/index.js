"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FulcioSigner = exports.DEFAULT_FULCIO_URL = void 0;
/*
Copyright 2023 The Sigstore Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
const error_1 = require("../../error");
const util_1 = require("../../util");
const ca_1 = require("./ca");
const ephemeral_1 = require("./ephemeral");
exports.DEFAULT_FULCIO_URL = 'https://fulcio.sigstore.dev';
// Signer implementation which can be used to decorate another signer
// with a Fulcio-issued signing certificate for the signer's public key.
// Must be instantiated with an identity provider which can provide a JWT
// which represents the identity to be bound to the signing certificate.
class FulcioSigner {
    constructor(options) {
        this.ca = new ca_1.CAClient({
            ...options,
            fulcioBaseURL: options.fulcioBaseURL || /* istanbul ignore next */ exports.DEFAULT_FULCIO_URL,
        });
        this.identityProvider = options.identityProvider;
        this.keyHolder = options.keyHolder || new ephemeral_1.EphemeralSigner();
    }
    async sign(data) {
        // Retrieve identity token from the supplied identity provider
        const identityToken = await this.getIdentityToken();
        // Extract challenge claim from OIDC token
        let subject;
        try {
            subject = util_1.oidc.extractJWTSubject(identityToken);
        }
        catch (err) {
            throw new error_1.InternalError({
                code: 'IDENTITY_TOKEN_PARSE_ERROR',
                message: `invalid identity token: ${identityToken}`,
                cause: err,
            });
        }
        // Construct challenge value by signing the subject claim
        const challenge = await this.keyHolder.sign(Buffer.from(subject));
        if (challenge.key.$case !== 'publicKey') {
            throw new error_1.InternalError({
                code: 'CA_CREATE_SIGNING_CERTIFICATE_ERROR',
                message: 'unexpected format for signing key',
            });
        }
        // Create signing certificate
        const certificates = await this.ca.createSigningCertificate(identityToken, challenge.key.publicKey, challenge.signature);
        // Generate artifact signature
        const signature = await this.keyHolder.sign(data);
        // Specifically returning only the first certificate in the chain
        // as the key.
        return {
            signature: signature.signature,
            key: {
                $case: 'x509Certificate',
                certificate: certificates[0],
            },
        };
    }
    async getIdentityToken() {
        try {
            return await this.identityProvider.getToken();
        }
        catch (err) {
            throw new error_1.InternalError({
                code: 'IDENTITY_TOKEN_READ_ERROR',
                message: 'error retrieving identity token',
                cause: err,
            });
        }
    }
}
exports.FulcioSigner = FulcioSigner;
