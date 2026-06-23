import type { Signature, Signer } from '../signer';
export declare class EphemeralSigner implements Signer {
    private keypair;
    constructor();
    sign(data: Buffer): Promise<Signature>;
}
