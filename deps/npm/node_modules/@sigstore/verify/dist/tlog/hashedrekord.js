"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.HASHEDREKORD_API_VERSION_V1 = void 0;
exports.verifyHashedRekordTLogBody = verifyHashedRekordTLogBody;
exports.verifyHashedRekordTLogBodyV2 = verifyHashedRekordTLogBodyV2;
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
exports.HASHEDREKORD_API_VERSION_V1 = '0.0.1';
// Compare the given hashedrekord tlog entry to the given bundle
function verifyHashedRekordTLogBody(tlogEntry, content) {
    switch (tlogEntry.apiVersion) {
        case exports.HASHEDREKORD_API_VERSION_V1:
            return verifyHashedrekord001TLogBody(tlogEntry, content);
        default:
            throw new error_1.VerificationError({
                code: 'TLOG_BODY_ERROR',
                message: `unsupported hashedrekord version: ${tlogEntry.apiVersion}`,
            });
    }
}
// Compare the given hashedrekor tlog entry to the given bundle. This function is
// specifically for Rekor V2 entries.
function verifyHashedRekordTLogBodyV2(tlogEntry, content) {
    const spec = tlogEntry.spec?.spec;
    if (!spec) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: `missing dsse spec`,
        });
    }
    switch (spec.$case) {
        case 'hashedRekordV002':
            return verifyHashedrekord002TLogBody(spec.hashedRekordV002, content);
        default:
            throw new error_1.VerificationError({
                code: 'TLOG_BODY_ERROR',
                message: `unsupported version: ${spec.$case}`,
            });
    }
}
// Compare the given hashedrekord v0.0.1 tlog entry to the given message
// signature
function verifyHashedrekord001TLogBody(tlogEntry, content) {
    // Ensure that the bundles message signature matches the tlog entry
    const tlogSig = tlogEntry.spec.signature.content || '';
    if (!content.compareSignature(Buffer.from(tlogSig, 'base64'))) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'signature mismatch',
        });
    }
    // Ensure that the bundle's message digest matches the tlog entry
    const tlogDigest = tlogEntry.spec.data.hash?.value || '';
    if (!content.compareDigest(Buffer.from(tlogDigest, 'hex'))) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'digest mismatch',
        });
    }
}
// Compare the given hashedrekord v0.0.2 tlog entry to the given message
// signature
function verifyHashedrekord002TLogBody(spec, content) {
    // Ensure that the bundles message signature matches the tlog entry
    const tlogSig = spec.signature?.content || Buffer.from('');
    if (!content.compareSignature(tlogSig)) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'signature mismatch',
        });
    }
    // Ensure that the bundle's message digest matches the tlog entry
    const tlogHash = spec.data?.digest || Buffer.from('');
    if (!content.compareDigest(tlogHash)) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: 'digest mismatch',
        });
    }
}
