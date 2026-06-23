import * as sigstore from '@sigstore/bundle';
import type { Signature } from '../signer';
import type { Artifact } from './base';
export declare function toMessageSignatureBundle(artifact: Artifact, signature: Signature): sigstore.BundleWithMessageSignature;
export declare function toDSSEBundle(artifact: Required<Artifact>, signature: Signature, certificateChain?: boolean): sigstore.BundleWithDsseEnvelope;
