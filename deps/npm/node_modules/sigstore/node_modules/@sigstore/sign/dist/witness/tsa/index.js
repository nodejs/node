"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.TSAWitness = void 0;
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
const client_1 = require("./client");
class TSAWitness {
    constructor(options) {
        this.tsa = new client_1.TSAClient({
            tsaBaseURL: options.tsaBaseURL,
            retry: options.retry,
            timeout: options.timeout,
        });
    }
    async testify(content) {
        const signature = extractSignature(content);
        const timestamp = await this.tsa.createTimestamp(signature);
        return {
            rfc3161Timestamps: [{ signedTimestamp: timestamp }],
        };
    }
}
exports.TSAWitness = TSAWitness;
function extractSignature(content) {
    switch (content.$case) {
        case 'dsseEnvelope':
            return content.dsseEnvelope.signatures[0].sig;
        case 'messageSignature':
            return content.messageSignature.signature;
    }
}
