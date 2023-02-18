import * as sigstore from '../../types/sigstore';
import { x509Certificate } from '../../x509/cert';
export declare function verifySignerIdentity(signingCert: x509Certificate, identities: sigstore.CertificateIdentities): void;
