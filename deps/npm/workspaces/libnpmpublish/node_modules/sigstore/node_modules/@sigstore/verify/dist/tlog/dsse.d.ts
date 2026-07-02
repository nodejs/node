import type { Entry } from '@sigstore/protobuf-specs/rekor/v2';
import type { ProposedDSSEEntry } from '@sigstore/rekor-types';
import type { SignatureContent } from '../shared.types';
export declare const DSSE_API_VERSION_V1 = "0.0.1";
export declare function verifyDSSETLogBody(tlogEntry: ProposedDSSEEntry, content: SignatureContent): void;
export declare function verifyDSSETLogBodyV2(tlogEntry: Entry, content: SignatureContent): void;
