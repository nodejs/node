"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.DSSE_API_VERSION_V1 = void 0;
exports.verifyDSSETLogBody = verifyDSSETLogBody;
exports.verifyDSSETLogBodyV2 = verifyDSSETLogBodyV2;
/*
Copyright 2025 The Sigstore Authors.

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
const error_1 = require("../error");
exports.DSSE_API_VERSION_V1 = '0.0.1';
// Compare the given dsse tlog entry to the given bundle
function verifyDSSETLogBody(tlogEntry, content) {
    switch (tlogEntry.apiVersion) {
        case exports.DSSE_API_VERSION_V1:
            return verifyDSSE001TLogBody(tlogEntry, content);
        default:
            throw new error_1.VerificationError({
                code: 'TLOG_BODY_ERROR',
                message: `unsupported dsse version: ${tlogEntry.apiVersion}`,
            });
    }
}
// Compare the given dsse tlog entry to the given bundle. This function is
// specifically for Rekor V2 entries.
function verifyDSSETLogBodyV2(tlogEntry, content) {
    const spec = tlogEntry.spec?.spec;
    if (!spec) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: `missing dsse spec`,
        });
    }
    switch (spec.$case) {
        case 'dsseV002':
            return verifyDSSE002TLogBody(spec.dsseV002, content);
        default:
            throw new error_1.VerificationError({
                code: 'TLOG_BODY_ERROR',
                message: `unsupported version: ${spec.$case}`,
            });
    }
}
// Compare the given dsse v0.0.1 tlog entry to the given DSSE envelope.
function verifyDSSE001TLogBody(tlogEntry, content) {
    // Ensure the bundle's DSSE only contains a single signature
    if (tlogEntry.spec.signatures?.length !== 1) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'signature count mismatch',
        });
    }
    const tlogSig = tlogEntry.spec.signatures[0].signature;
    // Ensure that the signature in the bundle's DSSE matches tlog entry
    if (!content.compareSignature(Buffer.from(tlogSig, 'base64')))
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'tlog entry signature mismatch',
        });
    // Ensure the digest of the bundle's DSSE payload matches the digest in the
    // tlog entry
    const tlogHash = tlogEntry.spec.payloadHash?.value || '';
    if (!content.compareDigest(Buffer.from(tlogHash, 'hex'))) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'DSSE payload hash mismatch',
        });
    }
}
// Compare the given dsse v0.0.2 tlog entry to the given DSSE envelope.
function verifyDSSE002TLogBody(spec, content) {
    // Ensure the bundle's DSSE only contains a single signature
    if (spec.signatures?.length !== 1) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'signature count mismatch',
        });
    }
    const tlogSig = spec.signatures[0].content;
    // Ensure that the signature in the bundle's DSSE matches tlog entry
    if (!content.compareSignature(tlogSig))
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'tlog entry signature mismatch',
        });
    // Ensure the digest of the bundle's DSSE payload matches the digest in the
    // tlog entry
    const tlogHash = spec.payloadHash?.digest || Buffer.from('');
    if (!content.compareDigest(tlogHash)) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'DSSE payload hash mismatch',
        });
    }
}
