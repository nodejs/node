import { Artifact, BaseBundleBuilder, BundleBuilderOptions } from './base';
import type { BundleWithDsseEnvelope } from '@sigstore/bundle';
import type { Signature } from '../signer';
type DSSEBundleBuilderOptions = BundleBuilderOptions & {
    certificateChain?: boolean;
};
export declare class DSSEBundleBuilder extends BaseBundleBuilder<BundleWithDsseEnvelope> {
    private certificateChain?;
    constructor(options: DSSEBundleBuilderOptions);
    protected prepare(artifact: Artifact): Promise<Buffer>;
    protected package(artifact: Artifact, signature: Signature): Promise<BundleWithDsseEnvelope>;
}
export {};
