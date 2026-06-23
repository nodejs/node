import type { Bundle, RFC3161SignedTimestamp, TransparencyLogEntry } from '@sigstore/bundle';
export type SignatureBundle = Bundle['content'];
export type VerificationMaterial = {
    tlogEntries?: TransparencyLogEntry[];
    rfc3161Timestamps?: RFC3161SignedTimestamp[];
};
export interface Witness {
    testify: (signature: SignatureBundle, publicKey: string) => Promise<VerificationMaterial>;
}
