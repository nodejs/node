"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.randomBytes = exports.hash = exports.verifyBlob = exports.signBlob = exports.createPublicKey = exports.generateKeyPair = void 0;
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
const crypto_1 = __importDefault(require("crypto"));
const EC_KEYPAIR_TYPE = 'ec';
const P256_CURVE = 'P-256';
const SHA256_ALGORITHM = 'sha256';
function generateKeyPair() {
    return crypto_1.default.generateKeyPairSync(EC_KEYPAIR_TYPE, {
        namedCurve: P256_CURVE,
    });
}
exports.generateKeyPair = generateKeyPair;
function createPublicKey(key) {
    if (typeof key === 'string') {
        return crypto_1.default.createPublicKey(key);
    }
    else {
        return crypto_1.default.createPublicKey({ key, format: 'der', type: 'spki' });
    }
}
exports.createPublicKey = createPublicKey;
function signBlob(data, privateKey) {
    return crypto_1.default.sign(null, data, privateKey);
}
exports.signBlob = signBlob;
function verifyBlob(data, key, signature, algorithm) {
    // The try/catch is to work around an issue in Node 14.x where verify throws
    // an error in some scenarios if the signature is invalid.
    try {
        return crypto_1.default.verify(algorithm, data, key, signature);
    }
    catch (e) {
        return false;
    }
}
exports.verifyBlob = verifyBlob;
function hash(data) {
    const hash = crypto_1.default.createHash(SHA256_ALGORITHM);
    return hash.update(data).digest();
}
exports.hash = hash;
function randomBytes(count) {
    return crypto_1.default.randomBytes(count);
}
exports.randomBytes = randomBytes;
