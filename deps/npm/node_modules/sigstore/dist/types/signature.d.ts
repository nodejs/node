/// <reference types="node" />
import { Envelope } from './sigstore';
import { OneOf } from './utility';
interface VerificationMaterial {
    certificates: string[];
    key: {
        id?: string;
        value: string;
    };
}
export type SignatureMaterial = {
    signature: Buffer;
} & OneOf<VerificationMaterial>;
export type SignerFunc = (payload: Buffer) => Promise<SignatureMaterial>;
export declare function extractSignatureMaterial(dsseEnvelope: Envelope, publicKey: string): SignatureMaterial;
export {};
