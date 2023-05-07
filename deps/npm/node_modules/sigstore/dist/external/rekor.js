"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Rekor = void 0;
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
const make_fetch_happen_1 = __importDefault(require("make-fetch-happen"));
const util_1 = require("../util");
const error_1 = require("./error");
/**
 * Rekor API client.
 */
class Rekor {
    constructor(options) {
        this.fetch = make_fetch_happen_1.default.defaults({
            retry: { retries: 2 },
            timeout: 5000,
            headers: {
                Accept: 'application/json',
                'User-Agent': util_1.ua.getUserAgent(),
            },
        });
        this.baseUrl = options.baseURL;
    }
    /**
     * Create a new entry in the Rekor log.
     * @param propsedEntry {EntryKind} Data to create a new entry
     * @returns {Promise<Entry>} The created entry
     */
    async createEntry(propsedEntry) {
        const url = `${this.baseUrl}/api/v1/log/entries`;
        const response = await this.fetch(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(propsedEntry),
        });
        (0, error_1.checkStatus)(response);
        const data = await response.json();
        return entryFromResponse(data);
    }
    /**
     * Get an entry from the Rekor log.
     * @param uuid {string} The UUID of the entry to retrieve
     * @returns {Promise<Entry>} The retrieved entry
     */
    async getEntry(uuid) {
        const url = `${this.baseUrl}/api/v1/log/entries/${uuid}`;
        const response = await this.fetch(url);
        (0, error_1.checkStatus)(response);
        const data = await response.json();
        return entryFromResponse(data);
    }
    /**
     * Search the Rekor log index for entries matching the given query.
     * @param opts {SearchIndex} Options to search the Rekor log
     * @returns {Promise<string[]>} UUIDs of matching entries
     */
    async searchIndex(opts) {
        const url = `${this.baseUrl}/api/v1/index/retrieve`;
        const response = await this.fetch(url, {
            method: 'POST',
            body: JSON.stringify(opts),
            headers: { 'Content-Type': 'application/json' },
        });
        (0, error_1.checkStatus)(response);
        const data = await response.json();
        return data;
    }
    /**
     * Search the Rekor logs for matching the given query.
     * @param opts {SearchLogQuery} Query to search the Rekor log
     * @returns {Promise<Entry[]>} List of matching entries
     */
    async searchLog(opts) {
        const url = `${this.baseUrl}/api/v1/log/entries/retrieve`;
        const response = await this.fetch(url, {
            method: 'POST',
            body: JSON.stringify(opts),
            headers: { 'Content-Type': 'application/json' },
        });
        (0, error_1.checkStatus)(response);
        const rawData = await response.json();
        const data = rawData.map((d) => entryFromResponse(d));
        return data;
    }
}
exports.Rekor = Rekor;
// Unpack the response from the Rekor API into a more convenient format.
function entryFromResponse(data) {
    const entries = Object.entries(data);
    if (entries.length != 1) {
        throw new Error('Received multiple entries in Rekor response');
    }
    // Grab UUID and entry data from the response
    const [uuid, entry] = Object.entries(data)[0];
    return {
        ...entry,
        uuid,
    };
}
