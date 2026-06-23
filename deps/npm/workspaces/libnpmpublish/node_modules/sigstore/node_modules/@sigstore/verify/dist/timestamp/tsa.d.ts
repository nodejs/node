import { RFC3161Timestamp } from '@sigstore/core';
import { CertAuthority } from '../trust';
export declare function verifyRFC3161Timestamp(timestamp: RFC3161Timestamp, data: Buffer, timestampAuthorities: CertAuthority[]): void;
