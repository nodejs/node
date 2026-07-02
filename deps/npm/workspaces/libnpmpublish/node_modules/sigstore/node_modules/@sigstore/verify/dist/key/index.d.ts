import { X509Certificate } from '@sigstore/core';
import { VerifiedSCTProvider } from './sct';
import type { Signer } from '../shared.types';
import type { TrustMaterial } from '../trust';
export type CertificateVerificationResult = {
    signer: Signer;
    scts: VerifiedSCTProvider[];
};
export declare function verifyPublicKey(hint: string, timestamps: Date[], trustMaterial: TrustMaterial): Signer;
export declare function verifyCertificate(leaf: X509Certificate, timestamps: Date[], trustMaterial: TrustMaterial): CertificateVerificationResult;
