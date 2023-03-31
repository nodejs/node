"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Fulcio = void 0;
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
 * Fulcio API client.
 */
class Fulcio {
    constructor(options) {
        this.fetch = make_fetch_happen_1.default.defaults({
            retry: { retries: 2 },
            timeout: 5000,
            headers: {
                'Content-Type': 'application/json',
                'User-Agent': util_1.ua.getUserAgent(),
            },
        });
        this.baseUrl = options.baseURL;
    }
    async createSigningCertificate(request) {
        const url = `${this.baseUrl}/api/v2/signingCert`;
        const response = await this.fetch(url, {
            method: 'POST',
            body: JSON.stringify(request),
        });
        (0, error_1.checkStatus)(response);
        const data = await response.json();
        return data;
    }
}
exports.Fulcio = Fulcio;
