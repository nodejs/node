/// <reference types="node" />
import { SignatureMaterial } from '../types/signature';
import * as sigstore from '../types/sigstore';
import type { Entry } from '../external/rekor';
import type { FetchOptions } from '../types/fetch';
interface CreateEntryOptions {
    fetchOnConflict?: boolean;
}
export interface TLog {
    createMessageSignatureEntry: (digest: Buffer, sigMaterial: SignatureMaterial) => Promise<Entry>;
    createDSSEEntry: (envelope: sigstore.Envelope, sigMaterial: SignatureMaterial, options?: CreateEntryOptions) => Promise<Entry>;
}
export type TLogClientOptions = {
    rekorBaseURL: string;
} & FetchOptions;
export declare class TLogClient implements TLog {
    private rekor;
    constructor(options: TLogClientOptions);
    createMessageSignatureEntry(digest: Buffer, sigMaterial: SignatureMaterial, options?: CreateEntryOptions): Promise<Entry>;
    createDSSEEntry(envelope: sigstore.Envelope, sigMaterial: SignatureMaterial, options?: CreateEntryOptions): Promise<Entry>;
    private createEntry;
}
export {};
