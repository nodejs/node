"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MessageSignatureContent = void 0;
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
const core_1 = require("@sigstore/core");
const protobuf_specs_1 = require("@sigstore/protobuf-specs");
// Map from the Sigstore protobuf HashAlgorithm enum to
// the string values used by the Node.js crypto module.
const HASH_ALGORITHM_MAP = {
    [protobuf_specs_1.HashAlgorithm.HASH_ALGORITHM_UNSPECIFIED]: 'sha256',
    [protobuf_specs_1.HashAlgorithm.SHA2_256]: 'sha256',
    [protobuf_specs_1.HashAlgorithm.SHA2_384]: 'sha384',
    [protobuf_specs_1.HashAlgorithm.SHA2_512]: 'sha512',
    [protobuf_specs_1.HashAlgorithm.SHA3_256]: 'sha3-256',
    [protobuf_specs_1.HashAlgorithm.SHA3_384]: 'sha3-384',
};
class MessageSignatureContent {
    signature;
    messageDigest;
    artifact;
    hashAlgorithm;
    constructor(messageSignature, artifact) {
        this.signature = messageSignature.signature;
        this.messageDigest = messageSignature.messageDigest.digest;
        this.artifact = artifact;
        this.hashAlgorithm =
            HASH_ALGORITHM_MAP[messageSignature.messageDigest.algorithm] ??
                /* istanbul ignore next */ 'sha256';
    }
    compareSignature(signature) {
        return core_1.crypto.bufferEqual(signature, this.signature);
    }
    compareDigest(digest) {
        return core_1.crypto.bufferEqual(digest, this.messageDigest);
    }
    verifySignature(key) {
        return core_1.crypto.verify(this.artifact, key, this.signature, this.hashAlgorithm);
    }
}
exports.MessageSignatureContent = MessageSignatureContent;
