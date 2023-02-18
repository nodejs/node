/// <reference types="node" />
import { SignatureMaterial } from '../types/signature';
import { Bundle, Envelope } from '../types/sigstore';
interface CreateEntryOptions {
    fetchOnConflict?: boolean;
}
export { Entry, EntryKind, HashedRekordKind } from './types';
export interface TLog {
    createMessageSignatureEntry: (digest: Buffer, sigMaterial: SignatureMaterial) => Promise<Bundle>;
    createDSSEEntry: (envelope: Envelope, sigMaterial: SignatureMaterial, options?: CreateEntryOptions) => Promise<Bundle>;
}
export interface TLogClientOptions {
    rekorBaseURL: string;
}
export declare class TLogClient implements TLog {
    private rekor;
    constructor(options: TLogClientOptions);
    createMessageSignatureEntry(digest: Buffer, sigMaterial: SignatureMaterial, options?: CreateEntryOptions): Promise<Bundle>;
    createDSSEEntry(envelope: Envelope, sigMaterial: SignatureMaterial, options?: CreateEntryOptions): Promise<Bundle>;
    private createEntry;
}
