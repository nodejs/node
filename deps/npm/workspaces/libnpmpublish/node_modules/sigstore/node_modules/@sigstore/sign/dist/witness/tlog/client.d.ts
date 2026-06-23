import type { TransparencyLogEntry } from '@sigstore/bundle';
import type { CreateEntryRequest } from '@sigstore/protobuf-specs/rekor/v2';
import type { Entry, ProposedEntry } from '../../external/rekor';
import type { FetchOptions } from '../../types/fetch';
export type { Entry, ProposedEntry };
export interface TLog {
    createEntry: (proposedEntry: ProposedEntry) => Promise<Entry>;
}
export type TLogClientOptions = {
    rekorBaseURL: string;
    fetchOnConflict?: boolean;
} & FetchOptions;
export declare class TLogClient implements TLog {
    private rekor;
    private fetchOnConflict;
    constructor(options: TLogClientOptions);
    createEntry(proposedEntry: ProposedEntry): Promise<Entry>;
}
export interface TLogV2 {
    createEntry: (createEntryRequest: CreateEntryRequest) => Promise<TransparencyLogEntry>;
}
export type TLogV2ClientOptions = {
    rekorBaseURL: string;
} & FetchOptions;
export declare class TLogV2Client implements TLogV2 {
    private rekor;
    constructor(options: TLogV2ClientOptions);
    createEntry(createEntryRequest: CreateEntryRequest): Promise<TransparencyLogEntry>;
}
