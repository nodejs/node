import { CAClientOptions } from './ca';
import type { IdentityProvider } from '../../identity';
import type { Signature, Signer } from '../signer';
export declare const DEFAULT_FULCIO_URL = "https://fulcio.sigstore.dev";
export type FulcioSignerOptions = {
    identityProvider: IdentityProvider;
    keyHolder?: Signer;
} & Partial<CAClientOptions>;
export declare class FulcioSigner implements Signer {
    private ca;
    private identityProvider;
    private keyHolder;
    constructor(options: FulcioSignerOptions);
    sign(data: Buffer): Promise<Signature>;
    private getIdentityToken;
}
