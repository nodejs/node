/// <reference types="node" />
import { LogId } from "./sigstore_common";
/** KindVersion contains the entry's kind and api version. */
export interface KindVersion {
    /**
     * Kind is the type of entry being stored in the log.
     * See here for a list: https://github.com/sigstore/rekor/tree/main/pkg/types
     */
    kind: string;
    /** The specific api version of the type. */
    version: string;
}
/**
 * The checkpoint contains a signature of the tree head (root hash),
 * size of the tree, the transparency log's unique identifier (log ID),
 * hostname and the current time.
 * The result is a string, the format is described here
 * https://github.com/transparency-dev/formats/blob/main/log/README.md
 * The details are here https://github.com/sigstore/rekor/blob/a6e58f72b6b18cc06cefe61808efd562b9726330/pkg/util/signed_note.go#L114
 * The signature has the same format as
 * InclusionPromise.signed_entry_timestamp. See below for more details.
 */
export interface Checkpoint {
    envelope: string;
}
/**
 * InclusionProof is the proof returned from the transparency log. Can
 * be used for on line verification against the log.
 */
export interface InclusionProof {
    /** The index of the entry in the log. */
    logIndex: string;
    /**
     * The hash digest stored at the root of the merkle tree at the time
     * the proof was generated.
     */
    rootHash: Buffer;
    /** The size of the merkle tree at the time the proof was generated. */
    treeSize: string;
    /**
     * A list of hashes required to compute the inclusion proof, sorted
     * in order from leaf to root.
     * Not that leaf and root hashes are not included.
     * The root has is available separately in this message, and the
     * leaf hash should be calculated by the client.
     */
    hashes: Buffer[];
    /**
     * Signature of the tree head, as of the time of this proof was
     * generated. See above info on 'Checkpoint' for more details.
     */
    checkpoint: Checkpoint | undefined;
}
/**
 * The inclusion promise is calculated by Rekor. It's calculated as a
 * signature over a canonical JSON serialization of the persisted entry, the
 * log ID, log index and the integration timestamp.
 * See https://github.com/sigstore/rekor/blob/a6e58f72b6b18cc06cefe61808efd562b9726330/pkg/api/entries.go#L54
 * The format of the signature depends on the transparency log's public key.
 * If the signature algorithm requires a hash function and/or a signature
 * scheme (e.g. RSA) those has to be retrieved out-of-band from the log's
 * operators, together with the public key.
 * This is used to verify the integration timestamp's value and that the log
 * has promised to include the entry.
 */
export interface InclusionPromise {
    signedEntryTimestamp: Buffer;
}
/**
 * TransparencyLogEntry captures all the details required from Rekor to
 * reconstruct an entry, given that the payload is provided via other means.
 * This type can easily be created from the existing response from Rekor.
 * Future iterations could rely on Rekor returning the minimal set of
 * attributes (excluding the payload) that are required for verifying the
 * inclusion promise. The inclusion promise (called SignedEntryTimestamp in
 * the response from Rekor) is similar to a Signed Certificate Timestamp
 * as described here https://www.rfc-editor.org/rfc/rfc9162#name-signed-certificate-timestam.
 */
export interface TransparencyLogEntry {
    /** The index of the entry in the log. */
    logIndex: string;
    /** The unique identifier of the log. */
    logId: LogId | undefined;
    /**
     * The kind (type) and version of the object associated with this
     * entry. These values are required to construct the entry during
     * verification.
     */
    kindVersion: KindVersion | undefined;
    /** The UNIX timestamp from the log when the entry was persisted. */
    integratedTime: string;
    /** The inclusion promise/signed entry timestamp from the log. */
    inclusionPromise: InclusionPromise | undefined;
    /**
     * The inclusion proof can be used for online verification that the
     * entry was appended to the log, and that the log has not been
     * altered.
     */
    inclusionProof: InclusionProof | undefined;
    /**
     * The canonicalized transparency log entry, used to reconstruct
     * the Signed Entry Timestamp (SET) during verification.
     * The contents of this field are the same as the `body` field in
     * a Rekor response, meaning that it does **not** include the "full"
     * canonicalized form (of log index, ID, etc.) which are
     * exposed as separate fields. The verifier is responsible for
     * combining the `canonicalized_body`, `log_index`, `log_id`,
     * and `integrated_time` into the payload that the SET's signature
     * is generated over.
     *
     * Clients MUST verify that the signatured referenced in the
     * `canonicalized_body` matches the signature provided in the
     * `Bundle.content`.
     */
    canonicalizedBody: Buffer;
}
export declare const KindVersion: {
    fromJSON(object: any): KindVersion;
    toJSON(message: KindVersion): unknown;
};
export declare const Checkpoint: {
    fromJSON(object: any): Checkpoint;
    toJSON(message: Checkpoint): unknown;
};
export declare const InclusionProof: {
    fromJSON(object: any): InclusionProof;
    toJSON(message: InclusionProof): unknown;
};
export declare const InclusionPromise: {
    fromJSON(object: any): InclusionPromise;
    toJSON(message: InclusionPromise): unknown;
};
export declare const TransparencyLogEntry: {
    fromJSON(object: any): TransparencyLogEntry;
    toJSON(message: TransparencyLogEntry): unknown;
};
