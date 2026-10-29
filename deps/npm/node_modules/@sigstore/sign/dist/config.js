"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.bundleBuilderFromSigningConfig = bundleBuilderFromSigningConfig;
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
const protobuf_specs_1 = require("@sigstore/protobuf-specs");
const dsse_1 = require("./bundler/dsse");
const message_1 = require("./bundler/message");
const signer_1 = require("./signer");
const witness_1 = require("./witness");
const MAX_CA_API_VERSION = 1;
const MAX_TLOG_API_VERSION = 2;
const MAX_TSA_API_VERSION = 1;
const DEFAULT_TIMEOUT = 5000;
const DEFAULT_REKORV2_TIMEOUT = 20000;
const DEFAULT_RETRY = { retries: 2 };
// Creates a BundleBuilder based on the provided SigningConfig
function bundleBuilderFromSigningConfig(options) {
    const { signingConfig, identityProvider, bundleType } = options;
    const fetchOptions = options.fetchOptions || {
        timeout: DEFAULT_TIMEOUT,
        retry: DEFAULT_RETRY,
    };
    const signer = fulcioSignerFromConfig(signingConfig, identityProvider, fetchOptions);
    const witnesses = witnessesFromConfig(signingConfig, fetchOptions);
    switch (bundleType) {
        case 'messageSignature':
            return new message_1.MessageSignatureBundleBuilder({ signer, witnesses });
        case 'dsseEnvelope':
            return new dsse_1.DSSEBundleBuilder({ signer, witnesses });
    }
}
function fulcioSignerFromConfig(signingConfig, identityProvider, fetchOptions) {
    const service = certAuthorityService(signingConfig);
    return new signer_1.FulcioSigner({
        fulcioBaseURL: service.url,
        identityProvider: identityProvider,
        timeout: fetchOptions.timeout,
        retry: fetchOptions.retry,
    });
}
function witnessesFromConfig(signingConfig, fetchOptions) {
    const witnesses = [];
    if (signingConfig.rekorTlogConfig) {
        if (signingConfig.rekorTlogConfig.selector !== protobuf_specs_1.ServiceSelector.ANY) {
            throw new Error('Unsupported Rekor TLog selector in signing configuration');
        }
        const tlog = tlogService(signingConfig);
        witnesses.push(new witness_1.RekorWitness({
            rekorBaseURL: tlog.url,
            majorApiVersion: tlog.majorApiVersion,
            retry: fetchOptions.retry,
            timeout:
            // Ensure Rekor V2 has at least a 20 second timeout
            tlog.majorApiVersion === 1
                ? fetchOptions.timeout
                : Math.min(fetchOptions.timeout ||
                    /* istanbul ignore next */ DEFAULT_TIMEOUT, DEFAULT_REKORV2_TIMEOUT),
        }));
    }
    if (signingConfig.tsaConfig) {
        if (signingConfig.tsaConfig.selector !== protobuf_specs_1.ServiceSelector.ANY) {
            throw new Error('Unsupported TSA selector in signing configuration');
        }
        const tsa = tsaService(signingConfig);
        witnesses.push(new witness_1.TSAWitness({
            tsaBaseURL: tsa.url,
            retry: fetchOptions.retry,
            timeout: fetchOptions.timeout,
        }));
    }
    return witnesses;
}
// Returns the first valid CA service from the signing configuration
function certAuthorityService(signingConfig) {
    const compatibleCAs = filterServicesByMaxAPIVersion(signingConfig.caUrls, MAX_CA_API_VERSION);
    const sortedCAs = sortServicesByStartDate(compatibleCAs);
    if (sortedCAs.length === 0) {
        throw new Error('No valid CA services found in signing configuration');
    }
    return sortedCAs[0];
}
// Returns the first valid TLog service from the signing configuration
function tlogService(signingConfig) {
    const compatibleTLogs = filterServicesByMaxAPIVersion(signingConfig.rekorTlogUrls, MAX_TLOG_API_VERSION);
    const sortedTLogs = sortServicesByStartDate(compatibleTLogs);
    if (sortedTLogs.length === 0) {
        throw new Error('No valid TLogs found in signing configuration');
    }
    return sortedTLogs[0];
}
// Returns the first valid TSA service from the signing configuration
function tsaService(signingConfig) {
    const compatibleTSAs = filterServicesByMaxAPIVersion(signingConfig.tsaUrls, MAX_TSA_API_VERSION);
    const sortedTSAs = sortServicesByStartDate(compatibleTSAs);
    if (sortedTSAs.length === 0) {
        throw new Error('No valid TSAs found in signing configuration');
    }
    return sortedTSAs[0];
}
// Returns the services sorted by start date (most recent first), filtering out
// any services that have an end date in the past
function sortServicesByStartDate(services) {
    const now = new Date();
    // Filter out any services that have an end date in the past
    const validServices = services.filter((service) => {
        // If there's no end date, the service is still valid
        if (!service.validFor?.end) {
            return true;
        }
        // Keep services whose end date is in the future or present
        return service.validFor.end >= now;
    });
    return validServices.sort((a, b) => {
        /* istanbul ignore next */
        const aStart = a.validFor?.start?.getTime() ?? 0;
        /* istanbul ignore next */
        const bStart = b.validFor?.start?.getTime() ?? 0;
        // Sort descending (most recent first)
        return bStart - aStart;
    });
}
// Returns a filtered list of services whose major API version is less than or
// equal to the specified version
function filterServicesByMaxAPIVersion(services, apiVersion) {
    // Filter out any services with a major API version greater than the specified version
    return services.filter((service) => {
        return service.majorApiVersion <= apiVersion;
    });
}
