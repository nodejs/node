import { Entry, EntryKind } from '../tlog';
export interface RekorOptions {
    baseURL: string;
}
export interface SearchIndex {
    email?: string;
    hash?: string;
}
export interface SearchLogQuery {
    entries?: EntryKind[];
    entryUUIDs?: string[];
    logIndexes?: number[];
}
/**
 * Rekor API client.
 */
export declare class Rekor {
    private fetch;
    private baseUrl;
    constructor(options: RekorOptions);
    /**
     * Create a new entry in the Rekor log.
     * @param propsedEntry {EntryKind} Data to create a new entry
     * @returns {Promise<Entry>} The created entry
     */
    createEntry(propsedEntry: EntryKind): Promise<Entry>;
    /**
     * Get an entry from the Rekor log.
     * @param uuid {string} The UUID of the entry to retrieve
     * @returns {Promise<Entry>} The retrieved entry
     */
    getEntry(uuid: string): Promise<Entry>;
    /**
     * Search the Rekor log index for entries matching the given query.
     * @param opts {SearchIndex} Options to search the Rekor log
     * @returns {Promise<string[]>} UUIDs of matching entries
     */
    searchIndex(opts: SearchIndex): Promise<string[]>;
    /**
     * Search the Rekor logs for matching the given query.
     * @param opts {SearchLogQuery} Query to search the Rekor log
     * @returns {Promise<Entry[]>} List of matching entries
     */
    searchLog(opts: SearchLogQuery): Promise<Entry[]>;
}
