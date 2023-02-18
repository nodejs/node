"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.createRekorEntry = exports.createDSSEEnvelope = void 0;
/*
Copyright 2022 The Sigstore Authors.

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
const sigstore_1 = require("./sigstore");
const tlog_1 = require("./tlog");
const signature_1 = require("./types/signature");
const sigstore_2 = require("./types/sigstore");
const util_1 = require("./util");
function createTLogClient(options) {
    return new tlog_1.TLogClient({
        rekorBaseURL: options.rekorURL || sigstore_1.DEFAULT_REKOR_URL,
    });
}
async function createDSSEEnvelope(payload, payloadType, options) {
    // Pre-authentication encoding to be signed
    const paeBuffer = util_1.dsse.preAuthEncoding(payloadType, payload);
    // Get signature and verification material for pae
    const sigMaterial = await options.signer(paeBuffer);
    const envelope = {
        payloadType,
        payload,
        signatures: [
            {
                keyid: sigMaterial.key?.id || '',
                sig: sigMaterial.signature,
            },
        ],
    };
    return (0, sigstore_2.envelopeToJSON)(envelope);
}
exports.createDSSEEnvelope = createDSSEEnvelope;
// Accepts a signed DSSE envelope and a PEM-encoded public key to be added to the
// transparency log. Returns a Sigstore bundle suitable for offline verification.
async function createRekorEntry(dsseEnvelope, publicKey, options = {}) {
    const envelope = (0, sigstore_2.envelopeFromJSON)(dsseEnvelope);
    const tlog = createTLogClient(options);
    const sigMaterial = (0, signature_1.extractSignatureMaterial)(envelope, publicKey);
    const bundle = await tlog.createDSSEEntry(envelope, sigMaterial, {
        fetchOnConflict: true,
    });
    return (0, sigstore_2.bundleToJSON)(bundle);
}
exports.createRekorEntry = createRekorEntry;
