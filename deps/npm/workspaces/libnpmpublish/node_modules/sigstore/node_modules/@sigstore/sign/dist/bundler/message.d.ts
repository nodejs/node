import { Artifact, BaseBundleBuilder, BundleBuilderOptions } from './base';
import type { BundleWithMessageSignature } from '@sigstore/bundle';
import type { Signature } from '../signer';
export declare class MessageSignatureBundleBuilder extends BaseBundleBuilder<BundleWithMessageSignature> {
    constructor(options: BundleBuilderOptions);
    protected package(artifact: Artifact, signature: Signature): Promise<BundleWithMessageSignature>;
}
