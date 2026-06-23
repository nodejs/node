import { X509Certificate } from '@sigstore/core';
import { CertAuthority } from '../trust';
export declare function verifyCertificateChain(timestamp: Date, leaf: X509Certificate, certificateAuthorities: CertAuthority[]): X509Certificate[];
interface CertificateChainVerifierOptions {
    trustedCerts: X509Certificate[];
    untrustedCert: X509Certificate;
    timestamp: Date;
}
export declare class CertificateChainVerifier {
    private untrustedCert;
    private trustedCerts;
    private localCerts;
    private timestamp;
    constructor(opts: CertificateChainVerifierOptions);
    verify(): X509Certificate[];
    private sort;
    private buildPaths;
    private findIssuer;
    private checkPath;
}
export {};
