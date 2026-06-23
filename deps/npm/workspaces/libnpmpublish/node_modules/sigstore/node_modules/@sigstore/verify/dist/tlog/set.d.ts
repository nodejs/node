import { TLogAuthority } from '../trust';
import type { TLogEntryWithInclusionPromise } from '@sigstore/bundle';
export declare function verifyTLogSET(entry: TLogEntryWithInclusionPromise, tlogs: TLogAuthority[]): void;
