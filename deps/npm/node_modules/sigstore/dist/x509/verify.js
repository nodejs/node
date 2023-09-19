"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifyCertificateChain = void 0;
/*
Copyright 2023 The Sigstore Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
const error_1 = require("../error");
function verifyCertificateChain(opts) {
    const verifier = new CertificateChainVerifier(opts);
    return verifier.verify();
}
exports.verifyCertificateChain = verifyCertificateChain;
class CertificateChainVerifier {
    constructor(opts) {
        this.untrustedCert = opts.untrustedCert;
        this.trustedCerts = opts.trustedCerts;
        this.localCerts = dedupeCertificates([
            ...opts.trustedCerts,
            opts.untrustedCert,
        ]);
        this.validAt = opts.validAt || new Date();
    }
    verify() {
        // Construct certificate path from leaf to root
        const certificatePath = this.sort();
        // Perform validation checks on each certificate in the path
        this.checkPath(certificatePath);
        // Return verified certificate path
        return certificatePath;
    }
    sort() {
        const leafCert = this.untrustedCert;
        // Construct all possible paths from the leaf
        let paths = this.buildPaths(leafCert);
        // Filter for paths which contain a trusted certificate
        paths = paths.filter((path) => path.some((cert) => this.trustedCerts.includes(cert)));
        if (paths.length === 0) {
            throw new error_1.VerificationError('No trusted certificate path found');
        }
        // Find the shortest of possible paths
        const path = paths.reduce((prev, curr) => prev.length < curr.length ? prev : curr);
        // Construct chain from shortest path
        // Removes the last certificate in the path, which will be a second copy
        // of the root certificate given that the root is self-signed.
        return [leafCert, ...path].slice(0, -1);
    }
    // Recursively build all possible paths from the leaf to the root
    buildPaths(certificate) {
        const paths = [];
        const issuers = this.findIssuer(certificate);
        if (issuers.length === 0) {
            throw new error_1.VerificationError('No valid certificate path found');
        }
        for (let i = 0; i < issuers.length; i++) {
            const issuer = issuers[i];
            // Base case - issuer is self
            if (issuer.equals(certificate)) {
                paths.push([certificate]);
                continue;
            }
            // Recursively build path for the issuer
            const subPaths = this.buildPaths(issuer);
            // Construct paths by appending the issuer to each subpath
            for (let j = 0; j < subPaths.length; j++) {
                paths.push([issuer, ...subPaths[j]]);
            }
        }
        return paths;
    }
    // Return all possible issuers for the given certificate
    findIssuer(certificate) {
        let issuers = [];
        let keyIdentifier;
        // Exit early if the certificate is self-signed
        if (certificate.subject.equals(certificate.issuer)) {
            if (certificate.verify()) {
                return [certificate];
            }
        }
        // If the certificate has an authority key identifier, use that
        // to find the issuer
        if (certificate.extAuthorityKeyID) {
            keyIdentifier = certificate.extAuthorityKeyID.keyIdentifier;
            // TODO: Add support for authorityCertIssuer/authorityCertSerialNumber
            // though Fulcio doesn't appear to use these
        }
        // Find possible issuers by comparing the authorityKeyID/subjectKeyID
        // or issuer/subject. Potential issuers are added to the result array.
        this.localCerts.forEach((possibleIssuer) => {
            if (keyIdentifier) {
                if (possibleIssuer.extSubjectKeyID) {
                    if (possibleIssuer.extSubjectKeyID.keyIdentifier.equals(keyIdentifier)) {
                        issuers.push(possibleIssuer);
                    }
                    return;
                }
            }
            // Fallback to comparing certificate issuer and subject if
            // subjectKey/authorityKey extensions are not present
            if (possibleIssuer.subject.equals(certificate.issuer)) {
                issuers.push(possibleIssuer);
            }
        });
        // Remove any issuers which fail to verify the certificate
        issuers = issuers.filter((issuer) => {
            try {
                return certificate.verify(issuer);
            }
            catch (ex) {
                return false;
            }
        });
        return issuers;
    }
    checkPath(path) {
        if (path.length < 1) {
            throw new error_1.VerificationError('Certificate chain must contain at least one certificate');
        }
        // Check that all certificates are valid at the check date
        const validForDate = path.every((cert) => cert.validForDate(this.validAt));
        if (!validForDate) {
            throw new error_1.VerificationError('Certificate is not valid or expired at the specified date');
        }
        // Ensure that all certificates beyond the leaf are CAs
        const validCAs = path.slice(1).every((cert) => cert.isCA);
        if (!validCAs) {
            throw new error_1.VerificationError('Intermediate certificate is not a CA');
        }
        // Certificate's issuer must match the subject of the next certificate
        // in the chain
        for (let i = path.length - 2; i >= 0; i--) {
            if (!path[i].issuer.equals(path[i + 1].subject)) {
                throw new error_1.VerificationError('Incorrect certificate name chaining');
            }
        }
        // Check pathlength constraints
        for (let i = 0; i < path.length; i++) {
            const cert = path[i];
            // If the certificate is a CA, check the path length
            if (cert.extBasicConstraints?.isCA) {
                const pathLength = cert.extBasicConstraints.pathLenConstraint;
                // The path length, if set, indicates how many intermediate
                // certificates (NOT including the leaf) are allowed to follow. The
                // pathLength constraint of any intermediate CA certificate MUST be
                // greater than or equal to it's own depth in the chain (with an
                // adjustment for the leaf certificate)
                if (pathLength !== undefined && pathLength < i - 1) {
                    throw new error_1.VerificationError('Path length constraint exceeded');
                }
            }
        }
    }
}
// Remove duplicate certificates from the array
function dedupeCertificates(certs) {
    for (let i = 0; i < certs.length; i++) {
        for (let j = i + 1; j < certs.length; j++) {
            if (certs[i].equals(certs[j])) {
                certs.splice(j, 1);
                j--;
            }
        }
    }
    return certs;
}
