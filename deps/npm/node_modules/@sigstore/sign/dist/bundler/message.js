"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MessageSignatureBundleBuilder = void 0;
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
const base_1 = require("./base");
const bundle_1 = require("./bundle");
// BundleBuilder implementation for raw message signatures
class MessageSignatureBundleBuilder extends base_1.BaseBundleBuilder {
    constructor(options) {
        super(options);
    }
    async package(artifact, signature) {
        return (0, bundle_1.toMessageSignatureBundle)(artifact, signature);
    }
}
exports.MessageSignatureBundleBuilder = MessageSignatureBundleBuilder;
