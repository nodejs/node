"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
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
const error_1 = require("../../error");
const sigstore = __importStar(require("../../types/sigstore"));
const body_1 = require("./body");
const set_1 = require("./set");
// Verifies that the number of tlog entries that pass offline verification
// is greater than or equal to the threshold specified in the options.
function verifyTLogEntries(bundle, trustedRoot, options) {
    if (options.performOnlineVerification) {
        throw new error_1.VerificationError('Online verification not implemented');
    }
    // Extract the signing cert, if available
    const signingCert = sigstore.signingCertificate(bundle);
    // Iterate over the tlog entries and verify each one
    const verifiedEntries = bundle.verificationMaterial.tlogEntries.filter((entry) => verifyTLogEntryOffline(entry, bundle.content, trustedRoot.tlogs, signingCert));
    if (verifiedEntries.length < options.threshold) {
        throw new error_1.VerificationError('tlog verification failed');
    }
}
exports.verifyTLogEntries = verifyTLogEntries;
function verifyTLogEntryOffline(entry, bundleContent, tlogs, signingCert) {
    // Check that the TLog entry has the fields necessary for verification
    if (!sigstore.isVerifiableTransparencyLogEntry(entry)) {
        return false;
    }
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
