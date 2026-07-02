import { SigningConfig } from '@sigstore/protobuf-specs';
import { BundleBuilder } from './bundler/base';
import { IdentityProvider } from './identity';
import type { MakeFetchHappenOptions } from 'make-fetch-happen';
type Retry = MakeFetchHappenOptions['retry'];
type BundleType = 'messageSignature' | 'dsseEnvelope';
type FetchOptions = {
    retry?: Retry;
    timeout?: number | undefined;
};
type Options = {
    signingConfig: SigningConfig;
    identityProvider: IdentityProvider;
    bundleType: BundleType;
    fetchOptions?: FetchOptions;
};
export declare function bundleBuilderFromSigningConfig(options: Options): BundleBuilder;
export {};
