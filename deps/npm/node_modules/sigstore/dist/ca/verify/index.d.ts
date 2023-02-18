import * as sigstore from '../../types/sigstore';
export declare function verifySigningCertificate(bundle: sigstore.BundleWithCertificateChain, trustedRoot: sigstore.TrustedRoot, options: sigstore.CAArtifactVerificationOptions): void;
