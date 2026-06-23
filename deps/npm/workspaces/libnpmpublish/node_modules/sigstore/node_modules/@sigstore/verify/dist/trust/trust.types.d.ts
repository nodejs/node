import type { X509Certificate, crypto } from '@sigstore/core';
export type TLogAuthority = {
    logID: Buffer;
    baseURL: string;
    publicKey: crypto.KeyObject;
    validFor: {
        start: Date;
        end: Date;
    };
};
export type CertAuthority = {
    certChain: X509Certificate[];
    validFor: {
        start: Date;
        end: Date;
    };
};
export type TimeConstrainedKey = {
    publicKey: crypto.KeyObject;
    validFor(date: Date): boolean;
};
export type KeyFinderFunc = (hint: string) => TimeConstrainedKey;
export type TrustMaterial = {
    certificateAuthorities: CertAuthority[];
    timestampAuthorities: CertAuthority[];
    tlogs: TLogAuthority[];
    ctlogs: TLogAuthority[];
    publicKey: KeyFinderFunc;
};
