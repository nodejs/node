"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifyCheckpoint = void 0;
const error_1 = require("../../error");
const util_1 = require("../../util");
// Separator between the note and the signatures in a checkpoint
const CHECKPOINT_SEPARATOR = '\n\n';
// Checkpoint signatures are of the following form:
// "– <identity> <key_hint+signature_bytes>\n"
// where:
// - the prefix is an emdash (U+2014).
// - <identity> gives a human-readable representation of the signing ID.
// - <key_hint+signature_bytes> is the first 4 bytes of the SHA256 hash of the
//   associated public key followed by the signature bytes.
const SIGNATURE_REGEX = /\u2014 (\S+) (\S+)\n/g;
// Verifies the checkpoint value in the given tlog entry. There are two steps
// to the verification:
// 1. Verify that all signatures in the checkpoint can be verified against a
//    trusted public key
// 2. Verify that the root hash in the checkpoint matches the root hash in the
//    inclusion proof
// See: https://github.com/transparency-dev/formats/blob/main/log/README.md
function verifyCheckpoint(entry, tlogs) {
    // Filter tlog instances to just those which were valid at the time of the
    // entry
    const validTLogs = filterTLogInstances(tlogs, entry.integratedTime);
    const inclusionProof = entry.inclusionProof;
    const signedNote = SignedNote.fromString(inclusionProof.checkpoint.envelope);
    const checkpoint = LogCheckpoint.fromString(signedNote.note);
    // Verify that the signatures in the checkpoint are all valid, also check
    // that the root hash from the checkpoint matches the root hash in the
    // inclusion proof
    return (signedNote.verify(validTLogs) &&
        util_1.crypto.bufferEqual(checkpoint.logHash, inclusionProof.rootHash));
}
exports.verifyCheckpoint = verifyCheckpoint;
// SignedNote represents a signed note from a transparency log checkpoint. Consists
// of a body (or note) and one more signatures calculated over the body. See
// https://github.com/transparency-dev/formats/blob/main/log/README.md#signed-envelope
class SignedNote {
    constructor(note, signatures) {
        this.note = note;
        this.signatures = signatures;
    }
    // Deserialize a SignedNote from a string
    static fromString(envelope) {
        if (!envelope.includes(CHECKPOINT_SEPARATOR)) {
            throw new error_1.VerificationError('malformed checkpoint: no separator');
        }
        // Split the note into the header and the data portions at the separator
        const split = envelope.indexOf(CHECKPOINT_SEPARATOR);
        const header = envelope.slice(0, split + 1);
        const data = envelope.slice(split + CHECKPOINT_SEPARATOR.length);
        // Find all the signature lines in the data portion
        const matches = data.matchAll(SIGNATURE_REGEX);
        // Parse each of the matched signature lines into the name and signature.
        // The first four bytes of the signature are the key hint (should match the
        // first four bytes of the log ID), and the rest is the signature itself.
        const signatures = Array.from(matches, (match) => {
            const [, name, signature] = match;
            const sigBytes = Buffer.from(signature, 'base64');
            if (sigBytes.length < 5) {
                throw new error_1.VerificationError('malformed checkpoint: invalid signature');
            }
            return {
                name,
                keyHint: sigBytes.subarray(0, 4),
                signature: sigBytes.subarray(4),
            };
        });
        if (signatures.length === 0) {
            throw new error_1.VerificationError('malformed checkpoint: no signatures');
        }
        return new SignedNote(header, signatures);
    }
    // Verifies the signatures in the SignedNote. For each signature, the
    // corresponding transparency log is looked up by the key hint and the
    // signature is verified against the public key in the transparency log.
    // Throws an error if any of the signatures are invalid.
    verify(tlogs) {
        const data = Buffer.from(this.note, 'utf-8');
        return this.signatures.every((signature) => {
            // Find the transparency log instance with the matching key hint
            const tlog = tlogs.find((tlog) => util_1.crypto.bufferEqual(tlog.logId.keyId.subarray(0, 4), signature.keyHint));
            if (!tlog) {
                return false;
            }
            const publicKey = util_1.crypto.createPublicKey(tlog.publicKey.rawBytes);
            return util_1.crypto.verifyBlob(data, publicKey, signature.signature);
        });
    }
}
// LogCheckpoint represents a transparency log checkpoint. Consists of the
// following:
//  - origin: the name of the transparency log
//  - logSize: the size of the log at the time of the checkpoint
//  - logHash: the root hash of the log at the time of the checkpoint
//  - rest: the rest of the checkpoint body, which is a list of log entries
// See:
// https://github.com/transparency-dev/formats/blob/main/log/README.md#checkpoint-body
class LogCheckpoint {
    constructor(origin, logSize, logHash, rest) {
        this.origin = origin;
        this.logSize = logSize;
        this.logHash = logHash;
        this.rest = rest;
    }
    static fromString(note) {
        const lines = note.trim().split('\n');
        if (lines.length < 4) {
            throw new error_1.VerificationError('malformed checkpoint: too few lines in header');
        }
        const origin = lines[0];
        const logSize = BigInt(lines[1]);
        const rootHash = Buffer.from(lines[2], 'base64');
        const rest = lines.slice(3);
        return new LogCheckpoint(origin, logSize, rootHash, rest);
    }
}
// Filter the list of tlog instances to only those which have usable public
// keys and were valid at the given time.
function filterTLogInstances(tlogInstances, integratedTime) {
    const targetDate = new Date(Number(integratedTime) * 1000);
    return tlogInstances.filter((tlog) => {
        // Must have a log ID
        if (!tlog.logId) {
            return false;
        }
        // If the tlog doesn't have a public key, we can't use it
        const publicKey = tlog.publicKey;
        if (publicKey === undefined) {
            return false;
        }
        // If the tlog doesn't have a rawBytes field, we can't use it
        if (publicKey.rawBytes === undefined) {
            return false;
        }
        // If the tlog doesn't have a validFor field, we don't need to check it
        const validFor = publicKey.validFor;
        if (validFor === undefined) {
            return true;
        }
        // Check that the integrated time is within the validFor range
        return (validFor.start !== undefined &&
            validFor.start <= targetDate &&
            (validFor.end === undefined || targetDate <= validFor.end));
    });
}
