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
exports.Verifier = void 0;
const ca = __importStar(require("./ca/verify"));
const error_1 = require("./error");
const tlog = __importStar(require("./tlog/verify"));
const sigstore = __importStar(require("./types/sigstore"));
const util_1 = require("./util");
class Verifier {
    constructor(trustedRoot, keySelector) {
        this.trustedRoot = trustedRoot;
        this.keySelector = keySelector || (() => undefined);
    }
    // Verifies the bundle signature, the bundle's certificate chain (if present)
    // and the bundle's transparency log entries.
    verify(bundle, options, data) {
        this.verifyArtifactSignature(bundle, data);
        if (sigstore.isBundleWithCertificateChain(bundle)) {
            this.verifySigningCertificate(bundle, options);
        }
        this.verifyTLogEntries(bundle, options);
    }
    // Performs bundle signature verification. Determines the type of the bundle
    // content and delegates to the appropriate signature verification function.
    verifyArtifactSignature(bundle, data) {
        const publicKey = this.getPublicKey(bundle);
        switch (bundle.content?.$case) {
            case 'messageSignature':
                if (!data) {
                    throw new error_1.VerificationError('no data provided for message signature verification');
                }
                verifyMessageSignature(data, bundle.content.messageSignature, publicKey);
                break;
            case 'dsseEnvelope':
                verifyDSSESignature(bundle.content.dsseEnvelope, publicKey);
                break;
        }
    }
    // Performs verification of the bundle's certificate chain. The bundle must
    // contain a certificate chain and the options must contain the required
    // options for CA verification.
    // TODO: We've temporarily removed the requirement that the options contain
    // the list of trusted signer identities. This will be added back in a future
    // release.
    verifySigningCertificate(bundle, options) {
        if (!sigstore.isCAVerificationOptions(options)) {
            throw new error_1.VerificationError('no trusted certificates provided for verification');
        }
        ca.verifySigningCertificate(bundle, this.trustedRoot, options);
    }
    // Performs verification of the bundle's transparency log entries. The bundle
    // must contain a list of transparency log entries.
    verifyTLogEntries(bundle, options) {
        tlog.verifyTLogEntries(bundle, this.trustedRoot, options.tlogOptions);
    }
    // Returns the public key which will be used to verify the bundle signature.
    // The public key is selected based on the verification material in the bundle
    // and the options provided.
    getPublicKey(bundle) {
        // Select the key which will be used to verify the signature
        switch (bundle.verificationMaterial?.content?.$case) {
            // If the bundle contains a certificate chain, the public key is the
            // first certificate in the chain (the signing certificate)
            case 'x509CertificateChain':
                return getPublicKeyFromCertificateChain(bundle.verificationMaterial.content.x509CertificateChain);
            // If the bundle contains a public key hint, the public key is selected
            // from the list of trusted keys in the options
            case 'publicKey':
                return getPublicKeyFromHint(bundle.verificationMaterial.content.publicKey, this.keySelector);
        }
    }
}
exports.Verifier = Verifier;
// Retrieves the public key from the first certificate in the certificate chain
function getPublicKeyFromCertificateChain(certificateChain) {
    const cert = util_1.pem.fromDER(certificateChain.certificates[0].rawBytes);
    return util_1.crypto.createPublicKey(cert);
}
// Retrieves the public key through the key selector callback, passing the
// public key hint from the bundle
function getPublicKeyFromHint(publicKeyID, keySelector) {
    const key = keySelector(publicKeyID.hint);
    if (!key) {
        throw new error_1.VerificationError('no public key found for signature verification');
    }
    try {
        return util_1.crypto.createPublicKey(key);
    }
    catch (e) {
        throw new error_1.VerificationError('invalid public key');
    }
}
// Performs signature verification for bundle containing a message signature.
// Verifies that the digest and signature found in the bundle match the
// provided data.
function verifyMessageSignature(data, messageSignature, publicKey) {
    // Extract signature for message
    const { signature, messageDigest } = messageSignature;
    const calculatedDigest = util_1.crypto.hash(data);
    if (!calculatedDigest.equals(messageDigest.digest)) {
        throw new error_1.VerificationError('message digest verification failed');
    }
    if (!util_1.crypto.verifyBlob(data, publicKey, signature)) {
        throw new error_1.VerificationError('artifact signature verification failed');
    }
}
// Performs signature verification for bundle containing a DSSE envelope.
// Calculates the PAE for the DSSE envelope and verifies it against the
// signature in the envelope.
function verifyDSSESignature(envelope, publicKey) {
    // Construct payload over which the signature was originally created
    const { payloadType, payload } = envelope;
    const data = util_1.dsse.preAuthEncoding(payloadType, payload);
    // Only support a single signature in DSSE
    const signature = envelope.signatures[0].sig;
    if (!util_1.crypto.verifyBlob(data, publicKey, signature)) {
        throw new error_1.VerificationError('artifact signature verification failed');
    }
}
