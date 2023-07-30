"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.identityProviders = exports.artifactVerificationOptions = exports.createTSAClient = exports.createTLogClient = exports.createCAClient = exports.DEFAULT_TIMEOUT = exports.DEFAULT_RETRY = exports.DEFAULT_REKOR_URL = exports.DEFAULT_FULCIO_URL = void 0;
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
const ca_1 = require("./ca");
const identity_1 = __importDefault(require("./identity"));
const tlog_1 = require("./tlog");
const tsa_1 = require("./tsa");
const sigstore = __importStar(require("./types/sigstore"));
exports.DEFAULT_FULCIO_URL = 'https://fulcio.sigstore.dev';
exports.DEFAULT_REKOR_URL = 'https://rekor.sigstore.dev';
exports.DEFAULT_RETRY = { retries: 2 };
exports.DEFAULT_TIMEOUT = 5000;
function createCAClient(options) {
    return new ca_1.CAClient({
        fulcioBaseURL: options.fulcioURL || exports.DEFAULT_FULCIO_URL,
        retry: options.retry ?? exports.DEFAULT_RETRY,
        timeout: options.timeout ?? exports.DEFAULT_TIMEOUT,
    });
}
exports.createCAClient = createCAClient;
function createTLogClient(options) {
    return new tlog_1.TLogClient({
        rekorBaseURL: options.rekorURL || exports.DEFAULT_REKOR_URL,
        retry: options.retry ?? exports.DEFAULT_RETRY,
        timeout: options.timeout ?? exports.DEFAULT_TIMEOUT,
    });
}
exports.createTLogClient = createTLogClient;
function createTSAClient(options) {
    return options.tsaServerURL
        ? new tsa_1.TSAClient({
            tsaBaseURL: options.tsaServerURL,
            retry: options.retry ?? exports.DEFAULT_RETRY,
            timeout: options.timeout ?? exports.DEFAULT_TIMEOUT,
        })
        : undefined;
}
exports.createTSAClient = createTSAClient;
// Assembles the AtifactVerificationOptions from the supplied VerifyOptions.
function artifactVerificationOptions(options) {
    // The trusted signers are only used if the options contain a certificate
    // issuer
    let signers;
    if (options.certificateIssuer) {
        let san = undefined;
        if (options.certificateIdentityEmail) {
            san = {
                type: sigstore.SubjectAlternativeNameType.EMAIL,
                identity: {
                    $case: 'value',
                    value: options.certificateIdentityEmail,
                },
            };
        }
        else if (options.certificateIdentityURI) {
            san = {
                type: sigstore.SubjectAlternativeNameType.URI,
                identity: {
                    $case: 'value',
                    value: options.certificateIdentityURI,
                },
            };
        }
        const oids = Object.entries(options.certificateOIDs || {}).map(([oid, value]) => ({
            oid: { id: oid.split('.').map((s) => parseInt(s, 10)) },
            value: Buffer.from(value),
        }));
        signers = {
            $case: 'certificateIdentities',
            certificateIdentities: {
                identities: [
                    {
                        issuer: options.certificateIssuer,
                        san: san,
                        oids: oids,
                    },
                ],
            },
        };
    }
    // Construct the artifact verification options w/ defaults
    return {
        ctlogOptions: {
            disable: false,
            threshold: options.ctLogThreshold || 1,
            detachedSct: false,
        },
        tlogOptions: {
            disable: false,
            threshold: options.tlogThreshold || 1,
            performOnlineVerification: false,
        },
        signers,
    };
}
exports.artifactVerificationOptions = artifactVerificationOptions;
// Translates the IdenityProviderOptions into a list of Providers which
// should be queried to retrieve an identity token.
function identityProviders(options) {
    const idps = [];
    const token = options.identityToken;
    // If an explicit identity token is provided, use that. Setup a dummy
    // provider that just returns the token. Otherwise, setup the CI context
    // provider and (optionally) the OAuth provider.
    if (token) {
        idps.push({ getToken: () => Promise.resolve(token) });
    }
    else {
        idps.push(identity_1.default.ciContextProvider());
        if (options.oidcIssuer && options.oidcClientID) {
            idps.push(identity_1.default.oauthProvider({
                issuer: options.oidcIssuer,
                clientID: options.oidcClientID,
                clientSecret: options.oidcClientSecret,
                redirectURL: options.oidcRedirectURL,
            }));
        }
    }
    return idps;
}
exports.identityProviders = identityProviders;
