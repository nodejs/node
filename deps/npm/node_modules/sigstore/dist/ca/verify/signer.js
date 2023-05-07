"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.verifySignerIdentity = void 0;
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
const error_1 = require("../../error");
const sigstore = __importStar(require("../../types/sigstore"));
// https://github.com/sigstore/fulcio/blob/main/docs/oid-info.md#1361415726411--issuer
const OID_FULCIO_ISSUER = '1.3.6.1.4.1.57264.1.1';
// https://github.com/sigstore/fulcio/blob/main/docs/oid-info.md#1361415726417--othername-san
const OID_FULCIO_USERNAME_SUBJECT = '1.3.6.1.4.1.57264.1.7';
// Verifies the identity embedded in a Fulcio-issued signing certificate against
// the list of trusted identities. Returns without error if at least one of the
// identities matches the signing certificate; otherwise, throws a
// VerificationError.
function verifySignerIdentity(signingCert, identities) {
    // Check that the signing certificate was issued to at least one of the
    // specified identities
    const signerVerified = identities.identities.some((identity) => verifyIdentity(signingCert, identity));
    if (!signerVerified) {
        throw new error_1.PolicyError('Certificate issued to untrusted signer');
    }
}
exports.verifySignerIdentity = verifySignerIdentity;
// Checks that the specified certificate was issued to the specified identity.
// The certificate must match the issuer, subject alternative name, and an
// optional list of certificate extensions. Returns true if the certificate was
// issued to the identity; otherwise, returns false.
function verifyIdentity(cert, identity) {
    return (verifyIssuer(cert, identity.issuer) &&
        verifySAN(cert, identity.san) &&
        verifyOIDs(cert, identity.oids));
}
// Checks the Fulcio issuer extension against the expected issuer. Returns true
// if the issuer matches; otherwise, returns false.
function verifyIssuer(cert, issuer) {
    const issuerExtension = cert.extension(OID_FULCIO_ISSUER);
    return issuerExtension?.value.toString('ascii') === issuer;
}
// Checks the certificate against the expected subject alternative name. Returns
// true if the SAN matches; otherwise, returns false.
function verifySAN(cert, expectedSAN) {
    // Fail if the SAN is not specified or is not a supported type
    if (expectedSAN === undefined ||
        expectedSAN.identity === undefined ||
        expectedSAN.type ===
            sigstore.SubjectAlternativeNameType
                .SUBJECT_ALTERNATIVE_NAME_TYPE_UNSPECIFIED) {
        return false;
    }
    const sanExtension = cert.extSubjectAltName;
    // Fail if the certificate does not have a SAN extension
    if (!sanExtension) {
        return false;
    }
    let sanValue;
    switch (expectedSAN.type) {
        case sigstore.SubjectAlternativeNameType.EMAIL:
            sanValue = sanExtension.rfc822Name;
            break;
        case sigstore.SubjectAlternativeNameType.URI:
            sanValue = sanExtension.uri;
            break;
        case sigstore.SubjectAlternativeNameType.OTHER_NAME:
            sanValue = sanExtension.otherName(OID_FULCIO_USERNAME_SUBJECT);
            break;
    }
    // Missing SAN value is an automatic failure
    if (sanValue === undefined) {
        return false;
    }
    let match;
    switch (expectedSAN.identity.$case) {
        case 'value':
            match = expectedSAN.identity.value;
            break;
        case 'regexp':
            // TODO support regex
            break;
    }
    return sanValue === match;
}
// Checks that the certificate contains the specified extensions. Returns true
// if all extensions are present and match the expected values; otherwise,
// returns false.
function verifyOIDs(cert, oids) {
    return oids.every((expectedExtension) => {
        if (!expectedExtension.oid) {
            return false;
        }
        const oid = expectedExtension.oid.id.join('.');
        const extension = cert.extension(oid);
        // If the extension is not present, or there is no value, return false
        const valueObj = extension?.valueObj;
        if (!valueObj) {
            return false;
        }
        // Check to see if this is a newer style extension with an embedded
        // UTF8String, or an older style extension with a raw string
        if (valueObj.subs.length > 0) {
            return valueObj.subs[0].value.equals(expectedExtension.value);
        }
        else {
            return valueObj.value.equals(expectedExtension.value);
        }
    });
}
