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
const config_1 = require("./config");
const signature_1 = require("./types/signature");
const sigstore = __importStar(require("./types/sigstore"));
const util_1 = require("./util");
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
    return sigstore.Envelope.toJSON(envelope);
}
exports.createDSSEEnvelope = createDSSEEnvelope;
// Accepts a signed DSSE envelope and a PEM-encoded public key to be added to the
// transparency log. Returns a Sigstore bundle suitable for offline verification.
async function createRekorEntry(dsseEnvelope, publicKey, options = {}) {
    const envelope = sigstore.Envelope.fromJSON(dsseEnvelope);
    const tlog = (0, config_1.createTLogClient)(options);
    const sigMaterial = (0, signature_1.extractSignatureMaterial)(envelope, publicKey);
    const entry = await tlog.createDSSEEntry(envelope, sigMaterial, {
        fetchOnConflict: true,
    });
    const bundle = sigstore.toDSSEBundle({
        envelope,
        signature: sigMaterial,
        tlogEntry: entry,
    });
    return sigstore.Bundle.toJSON(bundle);
}
exports.createRekorEntry = createRekorEntry;
