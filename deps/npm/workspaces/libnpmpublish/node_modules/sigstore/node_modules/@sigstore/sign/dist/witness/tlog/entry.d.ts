import type { CreateEntryRequest } from '@sigstore/protobuf-specs/rekor/v2';
import type { ProposedEntry } from '../../external/rekor';
import type { SignatureBundle } from '../witness';
export declare function toProposedEntry(content: SignatureBundle, publicKey: string, entryType?: 'dsse' | 'intoto'): ProposedEntry;
export declare function toCreateEntryRequest(content: SignatureBundle, publicKey: string): CreateEntryRequest;
