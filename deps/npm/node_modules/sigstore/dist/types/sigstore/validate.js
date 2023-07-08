"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.assertValidBundle = void 0;
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
// Performs basic validation of a Sigstore bundle to ensure that all required
// fields are populated. This is not a complete validation of the bundle, but
// rather a check that the bundle is in a valid state to be processed by the
// rest of the code.
function assertValidBundle(b) {
    const invalidValues = [];
    // Content-related validation
    if (b.content === undefined) {
        invalidValues.push('content');
    }
    else {
        switch (b.content.$case) {
            case 'messageSignature':
                if (b.content.messageSignature.messageDigest === undefined) {
                    invalidValues.push('content.messageSignature.messageDigest');
                }
                else {
                    if (b.content.messageSignature.messageDigest.digest.length === 0) {
                        invalidValues.push('content.messageSignature.messageDigest.digest');
                    }
                }
                if (b.content.messageSignature.signature.length === 0) {
                    invalidValues.push('content.messageSignature.signature');
                }
                break;
            case 'dsseEnvelope':
                if (b.content.dsseEnvelope.payload.length === 0) {
                    invalidValues.push('content.dsseEnvelope.payload');
                }
                if (b.content.dsseEnvelope.signatures.length !== 1) {
                    invalidValues.push('content.dsseEnvelope.signatures');
                }
                else {
                    if (b.content.dsseEnvelope.signatures[0].sig.length === 0) {
                        invalidValues.push('content.dsseEnvelope.signatures[0].sig');
                    }
                }
                break;
        }
    }
    // Verification material-related validation
    if (b.verificationMaterial === undefined) {
        invalidValues.push('verificationMaterial');
    }
    else {
        if (b.verificationMaterial.content === undefined) {
            invalidValues.push('verificationMaterial.content');
        }
        else {
            switch (b.verificationMaterial.content.$case) {
                case 'x509CertificateChain':
                    if (b.verificationMaterial.content.x509CertificateChain.certificates
                        .length === 0) {
                        invalidValues.push('verificationMaterial.content.x509CertificateChain.certificates');
                    }
                    b.verificationMaterial.content.x509CertificateChain.certificates.forEach((cert, i) => {
                        if (cert.rawBytes.length === 0) {
                            invalidValues.push(`verificationMaterial.content.x509CertificateChain.certificates[${i}].rawBytes`);
                        }
                    });
                    break;
            }
        }
    }
    if (invalidValues.length > 0) {
        throw new error_1.ValidationError(`invalid/missing bundle values: ${invalidValues.join(', ')}`);
    }
}
exports.assertValidBundle = assertValidBundle;
