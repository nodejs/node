import type { LogEntry, ProposedDSSEEntry, ProposedEntry, ProposedHashedRekordEntry, ProposedIntotoEntry } from '@sigstore/rekor-types';
import type { FetchOptions } from '../types/fetch';
export type { ProposedDSSEEntry, ProposedEntry, ProposedHashedRekordEntry, ProposedIntotoEntry, };
export type Entry = {
    uuid: string;
} & LogEntry[string];
export type RekorOptions = {
    baseURL: string;
} & FetchOptions;
/**
 * Rekor API client.
 */
export declare class Rekor {
    private options;
    constructor(options: RekorOptions);
    /**
     * Create a new entry in the Rekor log.
     * @param propsedEntry {ProposedEntry} Data to create a new entry
     * @returns {Promise<Entry>} The created entry
     */
    createEntry(propsedEntry: ProposedEntry): Promise<Entry>;
    /**
     * Get an entry from the Rekor log.
     * @param uuid {string} The UUID of the entry to retrieve
     * @returns {Promise<Entry>} The retrieved entry
     */
    getEntry(uuid: string): Promise<Entry>;
}
