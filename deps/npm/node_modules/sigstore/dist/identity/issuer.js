"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Issuer = void 0;
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
// Standard endpoint for retrieving OpenID configuration information
const OPENID_CONFIG_PATH = '/.well-known/openid-configuration';
/**
 * The Issuer reperesents a single OAuth2 provider.
 *
 * The Issuer is configured with a provider's base OAuth2 endpoint which is
 * used to retrieve the associated configuration information.
 */
class Issuer {
    constructor(baseURL) {
        this.baseURL = baseURL;
        this.fetch = make_fetch_happen_1.default.defaults({ retry: 2 });
    }
    async authEndpoint() {
        if (!this.config) {
            this.config = await this.loadOpenIDConfig();
        }
        return this.config.authorization_endpoint;
    }
    async tokenEndpoint() {
        if (!this.config) {
            this.config = await this.loadOpenIDConfig();
        }
        return this.config.token_endpoint;
    }
    async loadOpenIDConfig() {
        const url = `${this.baseURL}${OPENID_CONFIG_PATH}`;
        return this.fetch(url).then((res) => res.json());
    }
}
exports.Issuer = Issuer;
