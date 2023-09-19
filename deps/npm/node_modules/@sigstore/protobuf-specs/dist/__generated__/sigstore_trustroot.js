"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.TrustedRoot = exports.CertificateAuthority = exports.TransparencyLogInstance = void 0;
/* eslint-disable */
const sigstore_common_1 = require("./sigstore_common");
function createBaseTransparencyLogInstance() {
    return { baseUrl: "", hashAlgorithm: 0, publicKey: undefined, logId: undefined };
}
exports.TransparencyLogInstance = {
    fromJSON(object) {
        return {
            baseUrl: isSet(object.baseUrl) ? String(object.baseUrl) : "",
            hashAlgorithm: isSet(object.hashAlgorithm) ? (0, sigstore_common_1.hashAlgorithmFromJSON)(object.hashAlgorithm) : 0,
            publicKey: isSet(object.publicKey) ? sigstore_common_1.PublicKey.fromJSON(object.publicKey) : undefined,
            logId: isSet(object.logId) ? sigstore_common_1.LogId.fromJSON(object.logId) : undefined,
        };
    },
    toJSON(message) {
        const obj = {};
        message.baseUrl !== undefined && (obj.baseUrl = message.baseUrl);
        message.hashAlgorithm !== undefined && (obj.hashAlgorithm = (0, sigstore_common_1.hashAlgorithmToJSON)(message.hashAlgorithm));
        message.publicKey !== undefined &&
            (obj.publicKey = message.publicKey ? sigstore_common_1.PublicKey.toJSON(message.publicKey) : undefined);
        message.logId !== undefined && (obj.logId = message.logId ? sigstore_common_1.LogId.toJSON(message.logId) : undefined);
        return obj;
    },
};
function createBaseCertificateAuthority() {
    return { subject: undefined, uri: "", certChain: undefined, validFor: undefined };
}
exports.CertificateAuthority = {
    fromJSON(object) {
        return {
            subject: isSet(object.subject) ? sigstore_common_1.DistinguishedName.fromJSON(object.subject) : undefined,
            uri: isSet(object.uri) ? String(object.uri) : "",
            certChain: isSet(object.certChain) ? sigstore_common_1.X509CertificateChain.fromJSON(object.certChain) : undefined,
            validFor: isSet(object.validFor) ? sigstore_common_1.TimeRange.fromJSON(object.validFor) : undefined,
        };
    },
    toJSON(message) {
        const obj = {};
        message.subject !== undefined &&
            (obj.subject = message.subject ? sigstore_common_1.DistinguishedName.toJSON(message.subject) : undefined);
        message.uri !== undefined && (obj.uri = message.uri);
        message.certChain !== undefined &&
            (obj.certChain = message.certChain ? sigstore_common_1.X509CertificateChain.toJSON(message.certChain) : undefined);
        message.validFor !== undefined &&
            (obj.validFor = message.validFor ? sigstore_common_1.TimeRange.toJSON(message.validFor) : undefined);
        return obj;
    },
};
function createBaseTrustedRoot() {
    return { mediaType: "", tlogs: [], certificateAuthorities: [], ctlogs: [], timestampAuthorities: [] };
}
exports.TrustedRoot = {
    fromJSON(object) {
        return {
            mediaType: isSet(object.mediaType) ? String(object.mediaType) : "",
            tlogs: Array.isArray(object?.tlogs) ? object.tlogs.map((e) => exports.TransparencyLogInstance.fromJSON(e)) : [],
            certificateAuthorities: Array.isArray(object?.certificateAuthorities)
                ? object.certificateAuthorities.map((e) => exports.CertificateAuthority.fromJSON(e))
                : [],
            ctlogs: Array.isArray(object?.ctlogs)
                ? object.ctlogs.map((e) => exports.TransparencyLogInstance.fromJSON(e))
                : [],
            timestampAuthorities: Array.isArray(object?.timestampAuthorities)
                ? object.timestampAuthorities.map((e) => exports.CertificateAuthority.fromJSON(e))
                : [],
        };
    },
    toJSON(message) {
        const obj = {};
        message.mediaType !== undefined && (obj.mediaType = message.mediaType);
        if (message.tlogs) {
            obj.tlogs = message.tlogs.map((e) => e ? exports.TransparencyLogInstance.toJSON(e) : undefined);
        }
        else {
            obj.tlogs = [];
        }
        if (message.certificateAuthorities) {
            obj.certificateAuthorities = message.certificateAuthorities.map((e) => e ? exports.CertificateAuthority.toJSON(e) : undefined);
        }
        else {
            obj.certificateAuthorities = [];
        }
        if (message.ctlogs) {
            obj.ctlogs = message.ctlogs.map((e) => e ? exports.TransparencyLogInstance.toJSON(e) : undefined);
        }
        else {
            obj.ctlogs = [];
        }
        if (message.timestampAuthorities) {
            obj.timestampAuthorities = message.timestampAuthorities.map((e) => e ? exports.CertificateAuthority.toJSON(e) : undefined);
        }
        else {
            obj.timestampAuthorities = [];
        }
        return obj;
    },
};
function isSet(value) {
    return value !== null && value !== undefined;
}
