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
exports.Signer = void 0;
const sigstore = __importStar(require("./types/sigstore"));
const util_1 = require("./util");
class Signer {
    constructor(options) {
        this.identityProviders = [];
        this.ca = options.ca;
        this.tlog = options.tlog;
        this.tsa = options.tsa;
        this.identityProviders = options.identityProviders;
        this.tlogUpload = options.tlogUpload ?? true;
        this.signer = options.signer || this.signWithEphemeralKey.bind(this);
    }
    async signBlob(payload) {
        // Get signature and verification material for payload
        const sigMaterial = await this.signer(payload);
        // Calculate artifact digest
        const digest = util_1.crypto.hash(payload);
        // Create a Rekor entry (if tlogUpload is enabled)
        const entry = this.tlogUpload
            ? await this.tlog.createMessageSignatureEntry(digest, sigMaterial)
            : undefined;
        return sigstore.toMessageSignatureBundle({
            digest,
            signature: sigMaterial,
            tlogEntry: entry,
            timestamp: this.tsa
                ? await this.tsa.createTimestamp(sigMaterial.signature)
                : undefined,
        });
    }
    async signAttestation(payload, payloadType) {
        // Pre-authentication encoding to be signed
        const paeBuffer = util_1.dsse.preAuthEncoding(payloadType, payload);
        // Get signature and verification material for pae
        const sigMaterial = await this.signer(paeBuffer);
        const envelope = {
            payloadType,
            payload: payload,
            signatures: [
                {
                    keyid: sigMaterial.key?.id || '',
                    sig: sigMaterial.signature,
                },
            ],
        };
        // Create a Rekor entry (if tlogUpload is enabled)
        const entry = this.tlogUpload
            ? await this.tlog.createDSSEEntry(envelope, sigMaterial)
            : undefined;
        return sigstore.toDSSEBundle({
            envelope,
            signature: sigMaterial,
            tlogEntry: entry,
            timestamp: this.tsa
                ? await this.tsa.createTimestamp(sigMaterial.signature)
                : undefined,
        });
    }
    async signWithEphemeralKey(payload) {
        // Create emphemeral key pair
        const keypair = util_1.crypto.generateKeyPair();
        // Retrieve identity token from one of the supplied identity providers
        const identityToken = await this.getIdentityToken();
        // Extract challenge claim from OIDC token
        const subject = util_1.oidc.extractJWTSubject(identityToken);
        // Construct challenge value by encrypting subject with private key
        const challenge = util_1.crypto.signBlob(Buffer.from(subject), keypair.privateKey);
        // Create signing certificate
        const certificates = await this.ca.createSigningCertificate(identityToken, keypair.publicKey, challenge);
        // Generate artifact signature
        const signature = util_1.crypto.signBlob(payload, keypair.privateKey);
        return {
            signature,
            certificates,
            key: undefined,
        };
    }
    async getIdentityToken() {
        const aggErrs = [];
        for (const provider of this.identityProviders) {
            try {
                const token = await provider.getToken();
                if (token) {
                    return token;
                }
            }
            catch (err) {
                aggErrs.push(err);
            }
        }
        throw new Error(`Identity token providers failed: ${aggErrs}`);
    }
}
exports.Signer = Signer;
