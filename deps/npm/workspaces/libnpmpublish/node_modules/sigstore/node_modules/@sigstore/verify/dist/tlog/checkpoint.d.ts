import { TLogAuthority } from '../trust';
import type { TLogEntryWithInclusionProof } from '@sigstore/bundle';
export declare function verifyCheckpoint(entry: TLogEntryWithInclusionProof, tlogs: TLogAuthority[]): LogCheckpoint;
export declare class LogCheckpoint {
    readonly origin: string;
    readonly logSize: bigint;
    readonly logHash: Buffer;
    readonly rest: string[];
    constructor(origin: string, logSize: bigint, logHash: Buffer, rest: string[]);
    static fromString(note: string): LogCheckpoint;
}
