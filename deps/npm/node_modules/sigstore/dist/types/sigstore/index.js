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
var __exportStar = (this && this.__exportStar) || function(m, exports) {
    for (var p in m) if (p !== "default" && !Object.prototype.hasOwnProperty.call(exports, p)) __createBinding(exports, m, p);
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.signingCertificate = exports.bundle = exports.isVerifiableTransparencyLogEntry = exports.isCAVerificationOptions = exports.isBundleWithCertificateChain = exports.isBundleWithVerificationMaterial = exports.envelopeFromJSON = exports.envelopeToJSON = exports.bundleFromJSON = exports.bundleToJSON = exports.TransparencyLogEntry = void 0;
const util_1 = require("../../util");
const cert_1 = require("../../x509/cert");
const validate_1 = require("./validate");
const envelope_1 = require("./__generated__/envelope");
const sigstore_bundle_1 = require("./__generated__/sigstore_bundle");
const sigstore_common_1 = require("./__generated__/sigstore_common");
__exportStar(require("./serialized"), exports);
__exportStar(require("./validate"), exports);
__exportStar(require("./__generated__/envelope"), exports);
__exportStar(require("./__generated__/sigstore_bundle"), exports);
__exportStar(require("./__generated__/sigstore_common"), exports);
var sigstore_rekor_1 = require("./__generated__/sigstore_rekor");
Object.defineProperty(exports, "TransparencyLogEntry", { enumerable: true, get: function () { return sigstore_rekor_1.TransparencyLogEntry; } });
__exportStar(require("./__generated__/sigstore_trustroot"), exports);
__exportStar(require("./__generated__/sigstore_verification"), exports);
exports.bundleToJSON = sigstore_bundle_1.Bundle.toJSON;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
const bundleFromJSON = (obj) => {
    const bundle = sigstore_bundle_1.Bundle.fromJSON(obj);
    (0, validate_1.assertValidBundle)(bundle);
    return bundle;
};
exports.bundleFromJSON = bundleFromJSON;
exports.envelopeToJSON = envelope_1.Envelope.toJSON;
exports.envelopeFromJSON = envelope_1.Envelope.fromJSON;
const BUNDLE_MEDIA_TYPE = 'application/vnd.dev.sigstore.bundle+json;version=0.1';
// Type guard for narrowing a Bundle to a BundleWithVerificationMaterial
function isBundleWithVerificationMaterial(bundle) {
    return bundle.verificationMaterial !== undefined;
}
exports.isBundleWithVerificationMaterial = isBundleWithVerificationMaterial;
// Type guard for narrowing a Bundle to a BundleWithCertificateChain
function isBundleWithCertificateChain(bundle) {
    return (isBundleWithVerificationMaterial(bundle) &&
        bundle.verificationMaterial.content !== undefined &&
        bundle.verificationMaterial.content.$case === 'x509CertificateChain');
}
exports.isBundleWithCertificateChain = isBundleWithCertificateChain;
function isCAVerificationOptions(options) {
    return (options.ctlogOptions !== undefined &&
        (options.signers === undefined ||
            options.signers.$case === 'certificateIdentities'));
}
exports.isCAVerificationOptions = isCAVerificationOptions;
function isVerifiableTransparencyLogEntry(entry) {
    return (entry.logId !== undefined &&
        entry.inclusionPromise !== undefined &&
        entry.kindVersion !== undefined);
}
exports.isVerifiableTransparencyLogEntry = isVerifiableTransparencyLogEntry;
exports.bundle = {
    toDSSEBundle: (envelope, signature, rekorEntry) => ({
        mediaType: BUNDLE_MEDIA_TYPE,
        content: {
            $case: 'dsseEnvelope',
            dsseEnvelope: envelope,
        },
        verificationMaterial: toVerificationMaterial(signature, rekorEntry),
    }),
    toMessageSignatureBundle: (digest, signature, rekorEntry) => ({
        mediaType: BUNDLE_MEDIA_TYPE,
        content: {
            $case: 'messageSignature',
            messageSignature: {
                messageDigest: {
                    algorithm: sigstore_common_1.HashAlgorithm.SHA2_256,
                    digest: digest,
                },
                signature: signature.signature,
            },
        },
        verificationMaterial: toVerificationMaterial(signature, rekorEntry),
    }),
};
function toTransparencyLogEntry(entry) {
    const set = Buffer.from(entry.verification.signedEntryTimestamp, 'base64');
    const logID = Buffer.from(entry.logID, 'hex');
    // Parse entry body so we can extract the kind and version.
    const bodyJSON = util_1.encoding.base64Decode(entry.body);
    const entryBody = JSON.parse(bodyJSON);
    return {
        inclusionPromise: {
            signedEntryTimestamp: set,
        },
        logIndex: entry.logIndex.toString(),
        logId: {
            keyId: logID,
        },
        integratedTime: entry.integratedTime.toString(),
        kindVersion: {
            kind: entryBody.kind,
            version: entryBody.apiVersion,
        },
        inclusionProof: undefined,
        canonicalizedBody: Buffer.from(entry.body, 'base64'),
    };
}
function toVerificationMaterial(signature, entry) {
    return {
        content: signature.certificates
            ? toVerificationMaterialx509CertificateChain(signature.certificates)
            : toVerificationMaterialPublicKey(signature.key.id || ''),
        tlogEntries: [toTransparencyLogEntry(entry)],
        timestampVerificationData: undefined,
    };
}
function toVerificationMaterialx509CertificateChain(certificates) {
    return {
        $case: 'x509CertificateChain',
        x509CertificateChain: {
            certificates: certificates.map((c) => ({
                rawBytes: util_1.pem.toDER(c),
            })),
        },
    };
}
function toVerificationMaterialPublicKey(hint) {
    return { $case: 'publicKey', publicKey: { hint } };
}
function signingCertificate(bundle) {
    if (!isBundleWithCertificateChain(bundle)) {
        return undefined;
    }
    const signingCert = bundle.verificationMaterial.content.x509CertificateChain.certificates[0];
    return cert_1.x509Certificate.parse(signingCert.rawBytes);
}
exports.signingCertificate = signingCertificate;
