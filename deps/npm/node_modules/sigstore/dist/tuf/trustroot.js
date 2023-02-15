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
exports.TrustedRootFetcher = void 0;
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
const fs_1 = __importDefault(require("fs"));
const error_1 = require("../error");
const sigstore = __importStar(require("../types/sigstore"));
const util_1 = require("../util");
const TRUSTED_ROOT_MEDIA_TYPE = 'application/vnd.dev.sigstore.trustedroot+json;version=0.1';
// Type guard for SigstoreTargetMetadata
function isTargetMetadata(m) {
    return (m !== undefined &&
        m !== null &&
        typeof m === 'object' &&
        'status' in m &&
        'usage' in m &&
        'uri' in m);
}
class TrustedRootFetcher {
    constructor(tuf) {
        this.tuf = tuf;
    }
    // Assembles a TrustedRoot from the targets in the TUF repo
    async getTrustedRoot() {
        // Get all available targets
        const targets = await this.allTargets();
        const cas = await this.getCAKeys(targets, 'Fulcio');
        const ctlogs = await this.getTLogKeys(targets, 'CTFE');
        const tlogs = await this.getTLogKeys(targets, 'Rekor');
        return {
            mediaType: TRUSTED_ROOT_MEDIA_TYPE,
            certificateAuthorities: cas,
            ctlogs: ctlogs,
            tlogs: tlogs,
            timestampAuthorities: [],
        };
    }
    // Retrieves the list of TUF targets.
    // NOTE: This is a HACK to get around the fact that the TUF library doesn't
    // expose the list of targets. This is a temporary solution until TUF comes up
    // with a story for target discovery.
    // https://docs.google.com/document/d/1rWHAM2qCUtnjWD4lOrGWE2EIDLoA7eSy4-jB66Wgh0o
    async allTargets() {
        try {
            await this.tuf.refresh();
        }
        catch (e) {
            throw new error_1.InternalError('error refreshing trust metadata');
        }
        return Object.values(
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        this.tuf.trustedSet.targets?.signed.targets || {});
    }
    // Filters the supplied list of targets to those with the specified usage
    // and returns a new TransparencyLogInstance for each with the associated
    // public key populated.
    async getTLogKeys(targets, usage) {
        const filteredTargets = filterByUsage(targets, usage);
        return Promise.all(filteredTargets.map(async (target) => {
            const keyBytes = await this.readTargetBytes(target);
            const uri = isTargetMetadata(target.custom.sigstore)
                ? target.custom.sigstore.uri
                : '';
            // The log ID is not present in the Sigstore target metadata, but
            // can be derived by hashing the contents of the public key.
            return {
                baseUrl: uri,
                hashAlgorithm: sigstore.HashAlgorithm.SHA2_256,
                logId: { keyId: util_1.crypto.hash(keyBytes) },
                publicKey: {
                    keyDetails: sigstore.PublicKeyDetails.PKIX_ECDSA_P256_SHA_256,
                    rawBytes: keyBytes,
                },
            };
        }));
    }
    // Filters the supplied list of targets to those with the specified usage
    // and returns a new CertificateAuthority populated with all of the associated
    // certificates.
    // NOTE: The Sigstore target metadata does NOT provide any mechanism to link
    // related certificates (e.g. a root and intermediate). As a result, we
    // assume that all certificates located here are part of the same chain.
    // This works out OK since our certificate chain verification code tries all
    // possible permutations of the certificates until it finds one that results
    // in a valid, trusted chain.
    async getCAKeys(targets, usage) {
        const filteredTargets = filterByUsage(targets, usage);
        const certs = await Promise.all(filteredTargets.map(async (target) => await this.readTargetBytes(target)));
        return [
            {
                uri: '',
                subject: undefined,
                validFor: { start: new Date(0) },
                certChain: {
                    certificates: certs.map((cert) => ({ rawBytes: cert })),
                },
            },
        ];
    }
    // Reads the contents of the specified target file as a DER-encoded buffer.
    async readTargetBytes(target) {
        try {
            let path = await this.tuf.findCachedTarget(target);
            // An empty path here means the target has not been cached locally, or is
            // out of date. In either case, we need to download it.
            if (!path) {
                path = await this.tuf.downloadTarget(target);
            }
            const file = fs_1.default.readFileSync(path);
            return util_1.pem.toDER(file.toString('utf-8'));
        }
        catch (err) {
            throw new error_1.InternalError(`error reading key/certificate for ${target.path}`);
        }
    }
}
exports.TrustedRootFetcher = TrustedRootFetcher;
function filterByUsage(targets, usage) {
    return targets.filter((target) => {
        const meta = target.custom.sigstore;
        return isTargetMetadata(meta) && meta.usage === usage;
    });
}
