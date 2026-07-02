import type { TransparencyLogEntry } from '@sigstore/bundle';
import type { SignatureContent } from '../shared.types';
import { TLogAuthority } from '../trust';
export declare function verifyTLogBody(entry: TransparencyLogEntry, sigContent: SignatureContent): void;
export declare function verifyTLogInclusion(entry: TransparencyLogEntry, tlogAuthorities: TLogAuthority[]): void;
