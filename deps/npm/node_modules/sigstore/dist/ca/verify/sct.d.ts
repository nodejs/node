import * as sigstore from '../../types/sigstore';
import { x509Certificate } from '../../x509/cert';
export declare function verifySCTs(certificateChain: x509Certificate[], ctLogs: sigstore.TransparencyLogInstance[], options: sigstore.ArtifactVerificationOptions_CtlogOptions): void;
