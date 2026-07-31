"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifySubjectAlternativeName = verifySubjectAlternativeName;
exports.verifyExtensions = verifyExtensions;
exports.verifyOIDs = verifyOIDs;
const error_1 = require("./error");
// Verifies that the signer's SAN matches the policy identity. The
// policyIdentity is treated as a JavaScript regular expression pattern and
// tested against the full signerIdentity string. For exact matching, use
// anchored patterns (e.g. '^user@example\\.com$').
function verifySubjectAlternativeName(policyIdentity, signerIdentity) {
    if (signerIdentity === undefined || !signerIdentity.match(policyIdentity)) {
        throw new error_1.PolicyError({
            code: 'UNTRUSTED_SIGNER_ERROR',
            message: `certificate identity error - expected ${policyIdentity}, got ${signerIdentity}`,
        });
    }
}
function verifyExtensions(policyExtensions, signerExtensions = {}) {
    let key;
    for (key in policyExtensions) {
        if (signerExtensions[key] !== policyExtensions[key]) {
            throw new error_1.PolicyError({
                code: 'UNTRUSTED_SIGNER_ERROR',
                message: `invalid certificate extension - expected ${key}=${policyExtensions[key]}, got ${key}=${signerExtensions[key]}`,
            });
        }
    }
}
function verifyOIDs(policyOIDs, signerOIDs = []) {
    for (const policyOID of policyOIDs) {
        const match = signerOIDs.find((signerOID) => oidEquals(policyOID.oid?.id, signerOID.oid?.id) &&
            policyOID.value.equals(signerOID.value));
        if (!match) {
            const oid = policyOID.oid?.id.join('.') ?? '<unknown>';
            throw new error_1.PolicyError({
                code: 'UNTRUSTED_SIGNER_ERROR',
                message: `invalid certificate extension - missing OID ${oid}`,
            });
        }
    }
}
function oidEquals(a, b) {
    if (a === undefined || b === undefined) {
        return false;
    }
    return a.length === b.length && a.every((v, i) => v === b[i]);
}
