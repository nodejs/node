"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.EphemeralSigner = void 0;
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
const crypto_1 = require("crypto");
const EC_KEYPAIR_TYPE = 'ec';
const P256_CURVE = 'P-256';
// Signer implementation which uses an ephemeral keypair to sign artifacts.
// The private key lives only in memory and is tied to the lifetime of the
// EphemeralSigner instance.
class EphemeralSigner {
    keypair;
    constructor() {
        this.keypair = (0, crypto_1.generateKeyPairSync)(EC_KEYPAIR_TYPE, {
            namedCurve: P256_CURVE,
        });
    }
    async sign(data) {
        const signature = (0, crypto_1.sign)('sha256', data, this.keypair.privateKey);
        const publicKey = this.keypair.publicKey
            .export({ format: 'pem', type: 'spki' })
            .toString('ascii');
        return {
            signature: signature,
            key: { $case: 'publicKey', publicKey },
        };
    }
}
exports.EphemeralSigner = EphemeralSigner;
