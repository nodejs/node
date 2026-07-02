import { type PublicKey, type TrustedRoot } from '@sigstore/protobuf-specs';
import type { KeyFinderFunc, TrustMaterial } from './trust.types';
export { filterCertAuthorities, filterTLogAuthorities } from './filter';
export type { CertAuthority, KeyFinderFunc, TLogAuthority, TrustMaterial, } from './trust.types';
export declare function toTrustMaterial(root: TrustedRoot, keys?: Record<string, PublicKey> | KeyFinderFunc): TrustMaterial;
