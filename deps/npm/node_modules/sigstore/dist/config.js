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
Object.defineProperty(exports, "__esModule", { value: true });
exports.artifactVerificationOptions = exports.createBundleBuilder = exports.DEFAULT_TIMEOUT = exports.DEFAULT_RETRY = void 0;
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
const sign_1 = require("@sigstore/sign");
const sigstore = __importStar(require("./types/sigstore"));
exports.DEFAULT_RETRY = { retries: 2 };
exports.DEFAULT_TIMEOUT = 5000;
function createBundleBuilder(bundleType, options) {
    const bundlerOptions = {
        signer: initSigner(options),
        witnesses: initWitnesses(options),
    };
    switch (bundleType) {
        case 'messageSignature':
            return new sign_1.MessageSignatureBundleBuilder(bundlerOptions);
        case 'dsseEnvelope':
            return new sign_1.DSSEBundleBuilder(bundlerOptions);
    }
}
exports.createBundleBuilder = createBundleBuilder;
// Instantiate the FulcioSigner based on the supplied options.
function initSigner(options) {
    return new sign_1.FulcioSigner({
        fulcioBaseURL: options.fulcioURL,
        identityProvider: options.identityProvider || initIdentityProvider(options),
        retry: options.retry ?? exports.DEFAULT_RETRY,
        timeout: options.timeout ?? exports.DEFAULT_TIMEOUT,
    });
}
// Instantiate an identity provider based on the supplied options. If an
// explicit identity token is provided, use that. Otherwise, use the CI
// context provider.
function initIdentityProvider(options) {
    const token = options.identityToken;
    if (token) {
        return { getToken: () => Promise.resolve(token) };
    }
    else {
        return new sign_1.CIContextProvider('sigstore');
    }
}
// Instantiate a collection of witnesses based on the supplied options.
function initWitnesses(options) {
    const witnesses = [];
    if (isRekorEnabled(options)) {
        witnesses.push(new sign_1.RekorWitness({
            rekorBaseURL: options.rekorURL,
            fetchOnConflict: false,
            retry: options.retry ?? exports.DEFAULT_RETRY,
            timeout: options.timeout ?? exports.DEFAULT_TIMEOUT,
        }));
    }
    if (isTSAEnabled(options)) {
        witnesses.push(new sign_1.TSAWitness({
            tsaBaseURL: options.tsaServerURL,
            retry: options.retry ?? exports.DEFAULT_RETRY,
            timeout: options.timeout ?? exports.DEFAULT_TIMEOUT,
        }));
    }
    return witnesses;
}
// Type assertion to ensure that Rekor is enabled
function isRekorEnabled(options) {
    return options.tlogUpload !== false;
}
// Type assertion to ensure that TSA is enabled
function isTSAEnabled(options) {
    return options.tsaServerURL !== undefined;
}
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
        const oids = Object.entries(options.certificateOIDs || /* istanbul ignore next */ {}).map(([oid, value]) => ({
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
            disable: options.ctLogThreshold === 0,
            threshold: options.ctLogThreshold ?? 1,
            detachedSct: false,
        },
        tlogOptions: {
            disable: options.tlogThreshold === 0,
            threshold: options.tlogThreshold ?? 1,
            performOnlineVerification: false,
        },
        signers,
    };
}
exports.artifactVerificationOptions = artifactVerificationOptions;
