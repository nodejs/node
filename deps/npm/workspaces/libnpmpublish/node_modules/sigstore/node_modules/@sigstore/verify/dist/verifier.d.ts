import type { SignedEntity, Signer, VerificationPolicy } from './shared.types';
import type { TrustMaterial } from './trust';
/**
 * Configuration options for the verifier.
 *
 * @public
 */
export interface VerifierOptions {
    /** Minimum number of transparency log entries required for verification */
    tlogThreshold?: number;
    /** Minimum number of certificate transparency log entries required */
    ctlogThreshold?: number;
    /**
     * Minimum number of timestamp authority timestamps required for verification
     * @deprecated Use timestampThreshold instead
     */
    tsaThreshold?: number;
    /** Minimum number of timestamps required for verification */
    timestampThreshold?: number;
}
export declare class Verifier {
    private trustMaterial;
    private options;
    constructor(trustMaterial: TrustMaterial, options?: VerifierOptions);
    verify(entity: SignedEntity, policy?: VerificationPolicy): Signer;
    private verifyTimestamps;
    private verifySigningKey;
    private verifyTLogs;
    private verifySignature;
    private verifyPolicy;
}
