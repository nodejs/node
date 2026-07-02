import { RFC3161Timestamp } from '@sigstore/core';
import type { TransparencyLogEntry } from '@sigstore/bundle';
import type { CertAuthority } from '../trust';
export type TimestampType = 'transparency-log' | 'timestamp-authority';
export type TimestampVerificationResult = {
    type: TimestampType;
    logID: Buffer;
    timestamp: Date;
};
export declare function getTSATimestamp(timestamp: RFC3161Timestamp, data: Buffer, timestampAuthorities: CertAuthority[]): TimestampVerificationResult;
export declare function getTLogTimestamp(entry: TransparencyLogEntry): TimestampVerificationResult | undefined;
