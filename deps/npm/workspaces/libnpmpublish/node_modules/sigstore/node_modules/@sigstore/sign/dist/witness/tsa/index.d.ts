import { TSAClientOptions } from './client';
import type { RFC3161SignedTimestamp } from '@sigstore/bundle';
import type { SignatureBundle, Witness } from '../witness';
type RFC3161SignedTimestamps = {
    rfc3161Timestamps: RFC3161SignedTimestamp[];
};
export type TSAWitnessOptions = TSAClientOptions;
export declare class TSAWitness implements Witness {
    private tsa;
    constructor(options: TSAWitnessOptions);
    testify(content: SignatureBundle): Promise<RFC3161SignedTimestamps>;
}
export {};
