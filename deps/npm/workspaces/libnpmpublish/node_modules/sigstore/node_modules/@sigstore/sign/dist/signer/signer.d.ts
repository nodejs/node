export type KeyMaterial = {
    $case: 'x509Certificate';
    certificate: string;
} | {
    $case: 'publicKey';
    publicKey: string;
    hint?: string;
};
export type Signature = {
    signature: Buffer;
    key: KeyMaterial;
};
export interface Signer {
    sign: (data: Buffer) => Promise<Signature>;
}
