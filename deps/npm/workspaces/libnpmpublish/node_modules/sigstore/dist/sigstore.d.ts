import { SerializedBundle } from '@sigstore/bundle';
import { Signer } from '@sigstore/verify';
import * as config from './config';
export declare function sign(payload: Buffer, options?: config.SignOptions): Promise<SerializedBundle>;
export declare function attest(payload: Buffer, payloadType: string, options?: config.SignOptions): Promise<SerializedBundle>;
export declare function verify(bundle: SerializedBundle, options?: config.VerifyOptions): Promise<Signer>;
export declare function verify(bundle: SerializedBundle, data: Buffer, options?: config.VerifyOptions): Promise<Signer>;
export interface BundleVerifier {
    verify(bundle: SerializedBundle, data?: Buffer): Signer;
}
export declare function createVerifier(options?: config.VerifyOptions): Promise<BundleVerifier>;
