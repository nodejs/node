import type { Bundle as ProtoBundle } from '@sigstore/protobuf-specs';
import type { Bundle, BundleLatest, BundleV01 } from './bundle';
export declare function assertBundle(b: ProtoBundle): asserts b is Bundle;
export declare function assertBundleV01(b: ProtoBundle): asserts b is BundleV01;
export declare function isBundleV01(b: Bundle): b is BundleV01;
export declare function assertBundleV02(b: ProtoBundle): asserts b is BundleLatest;
export declare function assertBundleLatest(b: ProtoBundle): asserts b is BundleLatest;
