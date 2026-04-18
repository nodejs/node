"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifyTLogBody = verifyTLogBody;
exports.verifyTLogInclusion = verifyTLogInclusion;
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
const v2_1 = require("@sigstore/protobuf-specs/rekor/v2");
const error_1 = require("../error");
const dsse_1 = require("./dsse");
const hashedrekord_1 = require("./hashedrekord");
const intoto_1 = require("./intoto");
const checkpoint_1 = require("./checkpoint");
const merkle_1 = require("./merkle");
const set_1 = require("./set");
// Verifies that the given tlog entry matches the supplied signature content.
function verifyTLogBody(entry, sigContent) {
    const { kind, version } = entry.kindVersion;
    const body = JSON.parse(entry.canonicalizedBody.toString('utf8'));
    // validate body
    if (kind !== body.kind || version !== body.apiVersion) {
        throw new error_1.VerificationError({
            code: 'TLOG_BODY_ERROR',
            message: `kind/version mismatch - expected: ${kind}/${version}, received: ${body.kind}/${body.apiVersion}`,
        });
    }
    switch (kind) {
        case 'dsse':
            // Rekor V1 and V2 use incompatible types so we need to branch here based on version
            if (version == dsse_1.DSSE_API_VERSION_V1) {
                return (0, dsse_1.verifyDSSETLogBody)(body, sigContent);
            }
            else {
                const entryRekorV2 = v2_1.Entry.fromJSON(body);
                return (0, dsse_1.verifyDSSETLogBodyV2)(entryRekorV2, sigContent);
            }
        case 'intoto':
            return (0, intoto_1.verifyIntotoTLogBody)(body, sigContent);
        case 'hashedrekord':
            // Rekor V1 and V2 use incompatible types so we need to branch here based on version
            if (version == hashedrekord_1.HASHEDREKORD_API_VERSION_V1) {
                return (0, hashedrekord_1.verifyHashedRekordTLogBody)(body, sigContent);
            }
            else {
                const entryRekorV2 = v2_1.Entry.fromJSON(body);
                return (0, hashedrekord_1.verifyHashedRekordTLogBodyV2)(entryRekorV2, sigContent);
            }
        /* istanbul ignore next */
        default:
            throw new error_1.VerificationError({
                code: 'TLOG_BODY_ERROR',
                message: `unsupported kind: ${kind}`,
            });
    }
}
function verifyTLogInclusion(entry, tlogAuthorities) {
    let inclusionVerified = false;
    if (isTLogEntryWithInclusionPromise(entry)) {
        (0, set_1.verifyTLogSET)(entry, tlogAuthorities);
        inclusionVerified = true;
    }
    if (isTLogEntryWithInclusionProof(entry)) {
        const checkpoint = (0, checkpoint_1.verifyCheckpoint)(entry, tlogAuthorities);
        (0, merkle_1.verifyMerkleInclusion)(entry, checkpoint);
        inclusionVerified = true;
    }
    if (!inclusionVerified) {
        throw new error_1.VerificationError({
            code: 'TLOG_MISSING_INCLUSION_ERROR',
            message: 'inclusion could not be verified',
        });
    }
    return;
}
function isTLogEntryWithInclusionPromise(entry) {
    return entry.inclusionPromise !== undefined;
}
function isTLogEntryWithInclusionProof(entry) {
    return entry.inclusionProof !== undefined;
}
