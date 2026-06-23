import type { ProposedIntotoEntry } from '@sigstore/rekor-types';
import type { SignatureContent } from '../shared.types';
export declare function verifyIntotoTLogBody(tlogEntry: ProposedIntotoEntry, content: SignatureContent): void;
