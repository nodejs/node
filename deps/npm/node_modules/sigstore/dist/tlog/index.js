"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.TLogClient = void 0;
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
const client_1 = require("../client");
const error_1 = require("../client/error");
const error_2 = require("../error");
const sigstore_1 = require("../types/sigstore");
const format_1 = require("./format");
class TLogClient {
    constructor(options) {
        this.rekor = new client_1.Rekor({ baseURL: options.rekorBaseURL });
    }
    async createMessageSignatureEntry(digest, sigMaterial, options = {}) {
        const proposedEntry = (0, format_1.toProposedHashedRekordEntry)(digest, sigMaterial);
        const entry = await this.createEntry(proposedEntry, options.fetchOnConflict);
        return sigstore_1.bundle.toMessageSignatureBundle(digest, sigMaterial, entry);
    }
    async createDSSEEntry(envelope, sigMaterial, options = {}) {
        const proposedEntry = (0, format_1.toProposedIntotoEntry)(envelope, sigMaterial);
        const entry = await this.createEntry(proposedEntry, options.fetchOnConflict);
        return sigstore_1.bundle.toDSSEBundle(envelope, sigMaterial, entry);
    }
    async createEntry(proposedEntry, fetchOnConflict = false) {
        let entry;
        try {
            entry = await this.rekor.createEntry(proposedEntry);
        }
        catch (err) {
            // If the entry already exists, fetch it (if enabled)
            if (entryExistsError(err) && fetchOnConflict) {
                // Grab the UUID of the existing entry from the location header
                const uuid = err.location.split('/').pop() || '';
                try {
                    entry = await this.rekor.getEntry(uuid);
                }
                catch (err) {
                    throw new error_2.InternalError('error fetching tlog entry', err);
                }
            }
            else {
                throw new error_2.InternalError('error creating tlog entry', err);
            }
        }
        return entry;
    }
}
exports.TLogClient = TLogClient;
function entryExistsError(value) {
    return (value instanceof error_1.HTTPError &&
        value.statusCode === 409 &&
        value.location !== undefined);
}
