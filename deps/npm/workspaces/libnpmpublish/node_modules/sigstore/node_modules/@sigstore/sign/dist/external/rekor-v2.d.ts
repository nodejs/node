import { TransparencyLogEntry } from '@sigstore/protobuf-specs';
import { CreateEntryRequest } from '@sigstore/protobuf-specs/rekor/v2';
import type { FetchOptions } from '../types/fetch';
export type RekorOptions = {
    baseURL: string;
} & FetchOptions;
/**
 * Rekor API client.
 */
export declare class RekorV2 {
    private options;
    constructor(options: RekorOptions);
    createEntry(proposedEntry: CreateEntryRequest): Promise<TransparencyLogEntry>;
}
