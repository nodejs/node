import type { Bundle } from '@sigstore/bundle';
import type { Signature, Signer } from '../signer';
import type { Witness } from '../witness';
export interface BundleBuilderOptions {
    signer: Signer;
    witnesses: Witness[];
}
export interface Artifact {
    data: Buffer;
    type?: string;
}
export interface BundleBuilder {
    create: (artifact: Artifact) => Promise<Bundle>;
}
export declare abstract class BaseBundleBuilder<T extends Bundle> implements BundleBuilder {
    protected signer: Signer;
    private witnesses;
    constructor(options: BundleBuilderOptions);
    create(artifact: Artifact): Promise<T>;
    protected prepare(artifact: Artifact): Promise<Buffer>;
    protected abstract package(artifact: Artifact, signature: Signature): Promise<T>;
}
