"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.RekorV2 = void 0;
/*
Copyright 2025 The Sigstore Authors.

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
const fetch_1 = require("./fetch");
const protobuf_specs_1 = require("@sigstore/protobuf-specs");
const v2_1 = require("@sigstore/protobuf-specs/rekor/v2");
/**
 * Rekor API client.
 */
class RekorV2 {
    options;
    constructor(options) {
        this.options = options;
    }
    async createEntry(proposedEntry) {
        const { baseURL, timeout, retry } = this.options;
        const url = `${baseURL}/api/v2/log/entries`;
        const response = await (0, fetch_1.fetchWithRetry)(url, {
            headers: {
                'Content-Type': 'application/json',
                Accept: 'application/json',
            },
            body: JSON.stringify(v2_1.CreateEntryRequest.toJSON(proposedEntry)),
            timeout,
            retry,
        });
        return response.json().then((data) => protobuf_specs_1.TransparencyLogEntry.fromJSON(data));
    }
}
exports.RekorV2 = RekorV2;
