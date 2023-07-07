"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.CIContextProvider = void 0;
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
// Collection of all the CI-specific providers we have implemented
const providers = [getGHAToken, getEnv];
/**
 * CIContextProvider is a composite identity provider which will iterate
 * over all of the CI-specific providers and return the token from the first
 * one that resolves.
 */
class CIContextProvider {
    constructor(audience) {
        this.audience = audience;
    }
    // Invoke all registered ProviderFuncs and return the value of whichever one
    // resolves first.
    async getToken() {
        return util_1.promise
            .promiseAny(providers.map((getToken) => getToken(this.audience)))
            .catch(() => Promise.reject('CI: no tokens available'));
    }
}
exports.CIContextProvider = CIContextProvider;
/**
 * getGHAToken can retrieve an OIDC token when running in a GitHub Actions
 * workflow
 */
async function getGHAToken(audience) {
    // Check to see if we're running in GitHub Actions
    if (!process.env.ACTIONS_ID_TOKEN_REQUEST_URL ||
        !process.env.ACTIONS_ID_TOKEN_REQUEST_TOKEN) {
        return Promise.reject('no token available');
    }
    // Construct URL to request token w/ appropriate audience
    const url = new URL(process.env.ACTIONS_ID_TOKEN_REQUEST_URL);
    url.searchParams.append('audience', audience);
    const response = await (0, make_fetch_happen_1.default)(url.href, {
        retry: 2,
        headers: {
            Accept: 'application/json',
            Authorization: `Bearer ${process.env.ACTIONS_ID_TOKEN_REQUEST_TOKEN}`,
        },
    });
    return response.json().then((data) => data.value);
}
/**
 * getEnv can retrieve an OIDC token from an environment variable.
 * This matches the behavior of https://github.com/sigstore/cosign/tree/main/pkg/providers/envvar
 */
async function getEnv() {
    if (!process.env.SIGSTORE_ID_TOKEN) {
        return Promise.reject('no token available');
    }
    return process.env.SIGSTORE_ID_TOKEN;
}
