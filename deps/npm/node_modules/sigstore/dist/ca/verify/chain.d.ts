import * as sigstore from '../../types/sigstore';
import { x509Certificate } from '../../x509/cert';
export declare function verifyChain(bundleCerts: sigstore.X509Certificate[], certificateAuthorities: sigstore.CertificateAuthority[]): x509Certificate[];
