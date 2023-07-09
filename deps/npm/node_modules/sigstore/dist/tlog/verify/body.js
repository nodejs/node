"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifyTLogBody = void 0;
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
const TLOG_MISMATCH_ERROR_MSG = 'bundle content and tlog entry do not match';
// Compare the given tlog entry to the given bundle
function verifyTLogBody(entry, bundleContent) {
    const { kind, version } = entry.kindVersion;
    const body = JSON.parse(entry.canonicalizedBody.toString('utf8'));
    try {
        if (kind !== body.kind || version !== body.apiVersion) {
            throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
        }
        switch (body.kind) {
            case 'dsse':
                verifyDSSETLogBody(body, bundleContent);
                break;
            case 'intoto':
                verifyIntotoTLogBody(body, bundleContent);
                break;
            case 'hashedrekord':
                verifyHashedRekordTLogBody(body, bundleContent);
                break;
            default:
                throw new error_1.VerificationError(`unsupported kind in tlog entry: ${kind}`);
        }
        return true;
    }
    catch (e) {
        return false;
    }
}
exports.verifyTLogBody = verifyTLogBody;
// Compare the given intoto tlog entry to the given bundle
function verifyDSSETLogBody(tlogEntry, content) {
    if (content?.$case !== 'dsseEnvelope') {
        throw new error_1.VerificationError(`unsupported bundle content: ${content?.$case || 'unknown'}`);
    }
    const dsse = content.dsseEnvelope;
    switch (tlogEntry.apiVersion) {
        case '0.0.1':
            verifyDSSE001TLogBody(tlogEntry, dsse);
            break;
        default:
            throw new error_1.VerificationError(`unsupported dsse version: ${tlogEntry.apiVersion}`);
    }
}
// Compare the given intoto tlog entry to the given bundle
function verifyIntotoTLogBody(tlogEntry, content) {
    if (content?.$case !== 'dsseEnvelope') {
        throw new error_1.VerificationError(`unsupported bundle content: ${content?.$case || 'unknown'}`);
    }
    const dsse = content.dsseEnvelope;
    switch (tlogEntry.apiVersion) {
        case '0.0.2':
            verifyIntoto002TLogBody(tlogEntry, dsse);
            break;
        default:
            throw new error_1.VerificationError(`unsupported intoto version: ${tlogEntry.apiVersion}`);
    }
}
// Compare the given hashedrekord tlog entry to the given bundle
function verifyHashedRekordTLogBody(tlogEntry, content) {
    if (content?.$case !== 'messageSignature') {
        throw new error_1.VerificationError(`unsupported bundle content: ${content?.$case || 'unknown'}`);
    }
    const messageSignature = content.messageSignature;
    switch (tlogEntry.apiVersion) {
        case '0.0.1':
            verifyHashedrekor001TLogBody(tlogEntry, messageSignature);
            break;
        default:
            throw new error_1.VerificationError(`unsupported hashedrekord version: ${tlogEntry.apiVersion}`);
    }
}
// Compare the given dsse v0.0.1 tlog entry to the given DSSE envelope.
function verifyDSSE001TLogBody(tlogEntry, dsse) {
    // Collect all of the signatures from the DSSE envelope
    // Turns them into base64-encoded strings for comparison
    const dsseSigs = dsse.signatures.map((signature) => signature.sig.toString('base64'));
    // Collect all of the signatures from the tlog entry
    const tlogSigs = tlogEntry.spec.signatures?.map((signature) => signature.signature);
    // Ensure the bundle's DSSE and the tlog entry contain the same number of signatures
    if (dsseSigs.length !== tlogSigs?.length) {
        throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
    }
    // Ensure that every signature in the bundle's DSSE is present in the tlog entry
    if (!dsseSigs.every((dsseSig) => tlogSigs.includes(dsseSig))) {
        throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
    }
    // Ensure the digest of the bundle's DSSE payload matches the digest in the
    // tlog entry
    const dssePayloadHash = util_1.crypto.hash(dsse.payload).toString('hex');
    if (dssePayloadHash !== tlogEntry.spec.payloadHash?.value) {
        throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
    }
}
// Compare the given intoto v0.0.2 tlog entry to the given DSSE envelope.
function verifyIntoto002TLogBody(tlogEntry, dsse) {
    // Collect all of the signatures from the DSSE envelope
    // Turns them into base64-encoded strings for comparison
    const dsseSigs = dsse.signatures.map((signature) => signature.sig.toString('base64'));
    // Collect all of the signatures from the tlog entry
    // Remember that tlog signastures are double base64-encoded
    const tlogSigs = tlogEntry.spec.content.envelope?.signatures.map((signature) => (signature.sig ? util_1.encoding.base64Decode(signature.sig) : ''));
    // Ensure the bundle's DSSE and the tlog entry contain the same number of signatures
    if (dsseSigs.length !== tlogSigs?.length) {
        throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
    }
    // Ensure that every signature in the bundle's DSSE is present in the tlog entry
    if (!dsseSigs.every((dsseSig) => tlogSigs.includes(dsseSig))) {
        throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
    }
    // Ensure the digest of the bundle's DSSE payload matches the digest in the
    // tlog entry
    const dssePayloadHash = util_1.crypto.hash(dsse.payload).toString('hex');
    if (dssePayloadHash !== tlogEntry.spec.content.payloadHash?.value) {
        throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
    }
}
// Compare the given hashedrekord v0.0.1 tlog entry to the given message
// signature
function verifyHashedrekor001TLogBody(tlogEntry, messageSignature) {
    // Ensure that the bundles message signature matches the tlog entry
    const msgSig = messageSignature.signature.toString('base64');
    const tlogSig = tlogEntry.spec.signature.content;
    if (msgSig !== tlogSig) {
        throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
    }
    // Ensure that the bundle's message digest matches the tlog entry
    const msgDigest = messageSignature.messageDigest?.digest.toString('hex');
    const tlogDigest = tlogEntry.spec.data.hash?.value;
    if (msgDigest !== tlogDigest) {
        throw new error_1.VerificationError(TLOG_MISMATCH_ERROR_MSG);
    }
}
