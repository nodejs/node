/// <reference types="node" />
import * as sigstore from './types/sigstore';
export type KeySelector = (hint: string) => string | Buffer | undefined;
export declare class Verifier {
    private trustedRoot;
    private keySelector;
    constructor(trustedRoot: sigstore.TrustedRoot, keySelector?: KeySelector);
    verify(bundle: sigstore.ValidBundle, options: sigstore.RequiredArtifactVerificationOptions, data?: Buffer): void;
    private verifyArtifactSignature;
    private verifySigningCertificate;
    private verifyTLogEntries;
    private getPublicKey;
}
