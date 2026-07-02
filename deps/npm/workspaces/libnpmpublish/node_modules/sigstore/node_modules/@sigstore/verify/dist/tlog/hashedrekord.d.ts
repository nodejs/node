import { Entry } from '@sigstore/protobuf-specs/rekor/v2';
import type { ProposedHashedRekordEntry } from '@sigstore/rekor-types';
import type { SignatureContent } from '../shared.types';
export declare const HASHEDREKORD_API_VERSION_V1 = "0.0.1";
export declare function verifyHashedRekordTLogBody(tlogEntry: ProposedHashedRekordEntry, content: SignatureContent): void;
export declare function verifyHashedRekordTLogBodyV2(tlogEntry: Entry, content: SignatureContent): void;
