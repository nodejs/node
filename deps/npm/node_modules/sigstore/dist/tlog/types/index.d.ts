import { HashedRekorV001Schema } from './__generated__/hashedrekord';
import { IntotoV001Schema, IntotoV002Schema } from './__generated__/intoto';
export declare const INTOTO_KIND = "intoto";
export declare const HASHEDREKORD_KIND = "hashedrekord";
export type HashedRekordKind = {
    apiVersion: '0.0.1';
    kind: typeof HASHEDREKORD_KIND;
    spec: HashedRekorV001Schema;
};
export type IntotoKind = {
    apiVersion: '0.0.1';
    kind: typeof INTOTO_KIND;
    spec: IntotoV001Schema;
} | {
    apiVersion: '0.0.2';
    kind: typeof INTOTO_KIND;
    spec: IntotoV002Schema;
};
export type EntryKind = HashedRekordKind | IntotoKind;
export interface Entry {
    uuid: string;
    body: string;
    integratedTime: number;
    logID: string;
    logIndex: number;
    verification: EntryVerification;
    attestation?: object;
}
export interface EntryVerification {
    inclusionProof: InclusionProof;
    signedEntryTimestamp: string;
}
export interface InclusionProof {
    hashes: string[];
    logIndex: number;
    rootHash: string;
    treeSize: number;
}
