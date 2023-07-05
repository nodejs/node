import { x509Certificate } from './cert';
interface VerifyCertificateChainOptions {
    trustedCerts: x509Certificate[];
    untrustedCert: x509Certificate;
    validAt?: Date;
}
export declare function verifyCertificateChain(opts: VerifyCertificateChainOptions): x509Certificate[];
export {};
