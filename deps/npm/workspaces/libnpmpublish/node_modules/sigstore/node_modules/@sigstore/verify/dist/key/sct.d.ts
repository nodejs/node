import { X509Certificate } from '@sigstore/core';
import type { TLogAuthority } from '../trust';
export type VerifiedSCTProvider = Buffer;
export declare function verifySCTs(cert: X509Certificate, issuer: X509Certificate, ctlogs: TLogAuthority[]): VerifiedSCTProvider[];
