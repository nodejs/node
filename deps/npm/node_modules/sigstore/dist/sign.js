"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Signer = void 0;
const util_1 = require("./util");
class Signer {
    constructor(options) {
        this.identityProviders = [];
        this.ca = options.ca;
        this.tlog = options.tlog;
        this.identityProviders = options.identityProviders;
        this.signer = options.signer || this.signWithEphemeralKey.bind(this);
    }
    async signBlob(payload) {
        // Get signature and verification material for payload
        const sigMaterial = await this.signer(payload);
        // Calculate artifact digest
        const digest = util_1.crypto.hash(payload);
        // Create Rekor entry
        return this.tlog.createMessageSignatureEntry(digest, sigMaterial);
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
        return this.tlog.createDSSEEntry(envelope, sigMaterial);
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
