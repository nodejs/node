"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifyTLogEntries = void 0;
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
const bundle_1 = require("@sigstore/bundle");
const error_1 = require("../../error");
const cert_1 = require("../../x509/cert");
const body_1 = require("./body");
const checkpoint_1 = require("./checkpoint");
const merkle_1 = require("./merkle");
const set_1 = require("./set");
// Verifies that the number of tlog entries that pass offline verification
// is greater than or equal to the threshold specified in the options.
function verifyTLogEntries(bundle, trustedRoot, options) {
    if (bundle.mediaType === bundle_1.BUNDLE_V01_MEDIA_TYPE) {
        (0, bundle_1.assertBundleV01)(bundle);
        verifyTLogEntriesForBundleV01(bundle, trustedRoot, options);
    }
    else {
        (0, bundle_1.assertBundleLatest)(bundle);
        verifyTLogEntriesForBundleLatest(bundle, trustedRoot, options);
    }
}
exports.verifyTLogEntries = verifyTLogEntries;
function verifyTLogEntriesForBundleV01(bundle, trustedRoot, options) {
    if (options.performOnlineVerification) {
        throw new error_1.VerificationError('Online verification not implemented');
    }
    // Extract the signing cert, if available
    const signingCert = signingCertificate(bundle);
    // Iterate over the tlog entries and verify each one
    const verifiedEntries = bundle.verificationMaterial.tlogEntries.filter((entry) => verifyTLogEntryWithInclusionPromise(entry, bundle.content, trustedRoot.tlogs, signingCert));
    if (verifiedEntries.length < options.threshold) {
        throw new error_1.VerificationError('tlog verification failed');
    }
}
function verifyTLogEntriesForBundleLatest(bundle, trustedRoot, options) {
    if (options.performOnlineVerification) {
        throw new error_1.VerificationError('Online verification not implemented');
    }
    // Extract the signing cert, if available
    const signingCert = signingCertificate(bundle);
    // Iterate over the tlog entries and verify each one
    const verifiedEntries = bundle.verificationMaterial.tlogEntries.filter((entry) => verifyTLogEntryWithInclusionProof(entry, bundle.content, trustedRoot.tlogs, signingCert));
    if (verifiedEntries.length < options.threshold) {
        throw new error_1.VerificationError('tlog verification failed');
    }
}
function verifyTLogEntryWithInclusionPromise(entry, bundleContent, tlogs, signingCert) {
    // If there is a signing certificate availble, check that the tlog integrated
    // time is within the certificate's validity period; otherwise, skip this
    // check.
    const verifyTLogIntegrationTime = signingCert
        ? () => signingCert.validForDate(new Date(Number(entry.integratedTime) * 1000))
        : () => true;
    return ((0, body_1.verifyTLogBody)(entry, bundleContent) &&
        (0, set_1.verifyTLogSET)(entry, tlogs) &&
        verifyTLogIntegrationTime());
}
function verifyTLogEntryWithInclusionProof(entry, bundleContent, tlogs, signingCert) {
    // If there is a signing certificate availble, check that the tlog integrated
    // time is within the certificate's validity period; otherwise, skip this
    // check.
    const verifyTLogIntegrationTime = signingCert
        ? () => signingCert.validForDate(new Date(Number(entry.integratedTime) * 1000))
        : () => true;
    return ((0, body_1.verifyTLogBody)(entry, bundleContent) &&
        (0, merkle_1.verifyMerkleInclusion)(entry) &&
        (0, checkpoint_1.verifyCheckpoint)(entry, tlogs) &&
        verifyTLogIntegrationTime());
}
function signingCertificate(bundle) {
    if (!(0, bundle_1.isBundleWithCertificateChain)(bundle)) {
        return undefined;
    }
    const signingCert = bundle.verificationMaterial.content.x509CertificateChain.certificates[0];
    return cert_1.x509Certificate.parse(signingCert.rawBytes);
}
