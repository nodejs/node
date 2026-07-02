import { LogCheckpoint } from './checkpoint';
import type { TLogEntryWithInclusionProof } from '@sigstore/bundle';
export declare function verifyMerkleInclusion(entry: TLogEntryWithInclusionProof, checkpoint: LogCheckpoint): void;
