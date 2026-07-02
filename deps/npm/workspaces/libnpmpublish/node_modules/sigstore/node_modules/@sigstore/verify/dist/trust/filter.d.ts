import type { CertAuthority, TLogAuthority } from './trust.types';
export declare function filterCertAuthorities(certAuthorities: CertAuthority[], timestamp: Date): CertAuthority[];
type TLogAuthorityFilterCriteria = {
    targetDate: Date;
    logID?: Buffer;
};
export declare function filterTLogAuthorities(tlogAuthorities: TLogAuthority[], criteria: TLogAuthorityFilterCriteria): TLogAuthority[];
export {};
