import type { BundleWithDsseEnvelope, BundleWithMessageSignature } from './bundle';
type VerificationMaterialOptions = {
    certificate?: Buffer;
    keyHint?: string;
    certificateChain?: boolean;
};
type MessageSignatureBundleOptions = {
    digest: Buffer;
    signature: Buffer;
} & VerificationMaterialOptions;
type DSSEBundleOptions = {
    artifact: Buffer;
    artifactType: string;
    signature: Buffer;
} & VerificationMaterialOptions;
export declare function toMessageSignatureBundle(options: MessageSignatureBundleOptions): BundleWithMessageSignature;
export declare function toDSSEBundle(options: DSSEBundleOptions): BundleWithDsseEnvelope;
export {};
