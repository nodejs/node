"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
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
const ci_1 = require("./ci");
const issuer_1 = require("./issuer");
const oauth_1 = require("./oauth");
/**
 * oauthProvider returns a new Provider instance which attempts to retrieve
 * an identity token from the configured OAuth2 issuer.
 *
 * @param issuer Base URL of the issuer
 * @param clientID Client ID for the issuer
 * @param clientSecret Client secret for the issuer (optional)
 * @returns {Provider}
 */
function oauthProvider(options) {
    return new oauth_1.OAuthProvider({
        issuer: new issuer_1.Issuer(options.issuer),
        clientID: options.clientID,
        clientSecret: options.clientSecret,
        redirectURL: options.redirectURL,
    });
}
/**
 * ciContextProvider returns a new Provider instance which attempts to retrieve
 * an identity token from the CI context.
 *
 * @param audience audience claim for the generated token
 * @returns {Provider}
 */
function ciContextProvider(audience = 'sigstore') {
    return new ci_1.CIContextProvider(audience);
}
exports.default = {
    ciContextProvider,
    oauthProvider,
};
