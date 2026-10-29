"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.TLogV2Client = exports.TLogClient = void 0;
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
const error_1 = require("../../error");
const error_2 = require("../../external/error");
const rekor_1 = require("../../external/rekor");
const rekor_v2_1 = require("../../external/rekor-v2");
class TLogClient {
    rekor;
    fetchOnConflict;
    constructor(options) {
        this.fetchOnConflict = options.fetchOnConflict ?? false;
        this.rekor = new rekor_1.Rekor({
            baseURL: options.rekorBaseURL,
            retry: options.retry,
            timeout: options.timeout,
        });
    }
    async createEntry(proposedEntry) {
        let entry;
        try {
            entry = await this.rekor.createEntry(proposedEntry);
        }
        catch (err) {
            // If the entry already exists, fetch it (if enabled)
            if (entryExistsError(err) && this.fetchOnConflict) {
                // Grab the UUID of the existing entry from the location header
                /* istanbul ignore next */
                const uuid = err.location.split('/').pop() || '';
                try {
                    entry = await this.rekor.getEntry(uuid);
                }
                catch (err) {
                    (0, error_1.internalError)(err, 'TLOG_FETCH_ENTRY_ERROR', 'error fetching tlog entry');
                }
            }
            else {
                (0, error_1.internalError)(err, 'TLOG_CREATE_ENTRY_ERROR', 'error creating tlog entry');
            }
        }
        return entry;
    }
}
exports.TLogClient = TLogClient;
function entryExistsError(value) {
    return (value instanceof error_2.HTTPError &&
        value.statusCode === 409 &&
        value.location !== undefined);
}
class TLogV2Client {
    rekor;
    constructor(options) {
        this.rekor = new rekor_v2_1.RekorV2({
            baseURL: options.rekorBaseURL,
            retry: options.retry,
            timeout: options.timeout,
        });
    }
    async createEntry(createEntryRequest) {
        let entry;
        try {
            entry = await this.rekor.createEntry(createEntryRequest);
        }
        catch (err) {
            (0, error_1.internalError)(err, 'TLOG_CREATE_ENTRY_ERROR', 'error creating tlog entry');
        }
        if (entry.logId === undefined || entry.kindVersion === undefined) {
            (0, error_1.internalError)(new Error('invalid tlog entry'), 'TLOG_CREATE_ENTRY_ERROR', 'error creating tlog entry');
        }
        return {
            ...entry,
            logId: entry.logId,
            kindVersion: entry.kindVersion,
        };
    }
}
exports.TLogV2Client = TLogV2Client;
