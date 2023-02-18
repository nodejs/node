"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.x509Certificate = void 0;
const util_1 = require("../util");
const stream_1 = require("../util/stream");
const obj_1 = require("./asn1/obj");
const ext_1 = require("./ext");
const EXTENSION_OID_SUBJECT_KEY_ID = '2.5.29.14';
const EXTENSION_OID_KEY_USAGE = '2.5.29.15';
const EXTENSION_OID_SUBJECT_ALT_NAME = '2.5.29.17';
const EXTENSION_OID_BASIC_CONSTRAINTS = '2.5.29.19';
const EXTENSION_OID_AUTHORITY_KEY_ID = '2.5.29.35';
const EXTENSION_OID_SCT = '1.3.6.1.4.1.11129.2.4.2';
// List of recognized critical extensions
// https://www.rfc-editor.org/rfc/rfc5280#section-4.2
const RECOGNIZED_EXTENSIONS = [
    EXTENSION_OID_KEY_USAGE,
    EXTENSION_OID_BASIC_CONSTRAINTS,
    EXTENSION_OID_SUBJECT_ALT_NAME,
];
const ECDSA_SIGNATURE_ALGOS = {
    '1.2.840.10045.4.3.1': 'sha224',
    '1.2.840.10045.4.3.2': 'sha256',
    '1.2.840.10045.4.3.3': 'sha384',
    '1.2.840.10045.4.3.4': 'sha512',
};
class x509Certificate {
    constructor(asn1) {
        this.root = asn1;
        if (!this.checkRecognizedExtensions()) {
            throw new Error('Certificate contains unrecognized critical extensions');
        }
    }
    static parse(cert) {
        const der = typeof cert === 'string' ? util_1.pem.toDER(cert) : cert;
        const asn1 = obj_1.ASN1Obj.parseBuffer(der);
        return new x509Certificate(asn1);
    }
    get tbsCertificate() {
        return this.tbsCertificateObj;
    }
    get version() {
        // version number is the first element of the version context specific tag
        const ver = this.versionObj.subs[0].toInteger();
        return `v${(ver + BigInt(1)).toString()}`;
    }
    get notBefore() {
        // notBefore is the first element of the validity sequence
        return this.validityObj.subs[0].toDate();
    }
    get notAfter() {
        // notAfter is the second element of the validity sequence
        return this.validityObj.subs[1].toDate();
    }
    get issuer() {
        return this.issuerObj.value;
    }
    get subject() {
        return this.subjectObj.value;
    }
    get publicKey() {
        return this.subjectPublicKeyInfoObj.raw;
    }
    get signatureAlgorithm() {
        const oid = this.signatureAlgorithmObj.subs[0].toOID();
        return ECDSA_SIGNATURE_ALGOS[oid];
    }
    get signatureValue() {
        // Signature value is a bit string, so we need to skip the first byte
        return this.signatureValueObj.value.subarray(1);
    }
    get extensions() {
        // The extension list is the first (and only) element of the extensions
        // context specific tag
        const extSeq = this.extensionsObj?.subs[0];
        return extSeq?.subs || [];
    }
    get extKeyUsage() {
        const ext = this.findExtension(EXTENSION_OID_KEY_USAGE);
        return ext ? new ext_1.x509KeyUsageExtension(ext) : undefined;
    }
    get extBasicConstraints() {
        const ext = this.findExtension(EXTENSION_OID_BASIC_CONSTRAINTS);
        return ext ? new ext_1.x509BasicConstraintsExtension(ext) : undefined;
    }
    get extSubjectAltName() {
        const ext = this.findExtension(EXTENSION_OID_SUBJECT_ALT_NAME);
        return ext ? new ext_1.x509SubjectAlternativeNameExtension(ext) : undefined;
    }
    get extAuthorityKeyID() {
        const ext = this.findExtension(EXTENSION_OID_AUTHORITY_KEY_ID);
        return ext ? new ext_1.x509AuthorityKeyIDExtension(ext) : undefined;
    }
    get extSubjectKeyID() {
        const ext = this.findExtension(EXTENSION_OID_SUBJECT_KEY_ID);
        return ext ? new ext_1.x509SubjectKeyIDExtension(ext) : undefined;
    }
    get extSCT() {
        const ext = this.findExtension(EXTENSION_OID_SCT);
        return ext ? new ext_1.x509SCTExtension(ext) : undefined;
    }
    get isCA() {
        const ca = this.extBasicConstraints?.isCA || false;
        // If the KeyUsage extension is present, keyCertSign must be set
        if (this.extKeyUsage) {
            ca && this.extKeyUsage.keyCertSign;
        }
        return ca;
    }
    extension(oid) {
        const ext = this.findExtension(oid);
        return ext ? new ext_1.x509Extension(ext) : undefined;
    }
    verify(issuerCertificate) {
        // Use the issuer's public key if provided, otherwise use the subject's
        const publicKey = issuerCertificate?.publicKey || this.publicKey;
        const key = util_1.crypto.createPublicKey(publicKey);
        return util_1.crypto.verifyBlob(this.tbsCertificate.raw, key, this.signatureValue, this.signatureAlgorithm);
    }
    validForDate(date) {
        return this.notBefore <= date && date <= this.notAfter;
    }
    equals(other) {
        return this.root.raw.equals(other.root.raw);
    }
    verifySCTs(issuer, logs) {
        let extSCT;
        // Verifying the SCT requires that we remove the SCT extension and
        // re-encode the TBS structure to DER -- this value is part of the data
        // over which the signature is calculated. Since this is a destructive action
        // we create a copy of the certificate so we can remove the SCT extension
        // without affecting the original certificate.
        const clone = this.clone();
        // Intentionally not using the findExtension method here because we want to
        // remove the the SCT extension from the certificate before calculating the
        // PreCert structure
        for (let i = 0; i < clone.extensions.length; i++) {
            const ext = clone.extensions[i];
            if (ext.subs[0].toOID() === EXTENSION_OID_SCT) {
                extSCT = new ext_1.x509SCTExtension(ext);
                // Remove the extension from the certificate
                clone.extensions.splice(i, 1);
                break;
            }
        }
        if (!extSCT) {
            throw new Error('Certificate does not contain SCT extension');
        }
        if (extSCT?.signedCertificateTimestamps?.length === 0) {
            throw new Error('Certificate does not contain any SCTs');
        }
        // Construct the PreCert structure
        // https://www.rfc-editor.org/rfc/rfc6962#section-3.2
        const preCert = new stream_1.ByteStream();
        // Calculate hash of the issuer's public key
        const issuerId = util_1.crypto.hash(issuer.publicKey);
        preCert.appendView(issuerId);
        // Re-encodes the certificate to DER after removing the SCT extension
        const tbs = clone.tbsCertificate.toDER();
        preCert.appendUint24(tbs.length);
        preCert.appendView(tbs);
        // Calculate and return the verification results for each SCT
        return extSCT.signedCertificateTimestamps.map((sct) => ({
            logID: sct.logID,
            verified: sct.verify(preCert.buffer, logs),
        }));
    }
    // Creates a copy of the certificate with a new buffer
    clone() {
        const clone = Buffer.alloc(this.root.raw.length);
        this.root.raw.copy(clone);
        return x509Certificate.parse(clone);
    }
    findExtension(oid) {
        // Find the extension with the given OID. The OID will always be the first
        // element of the extension sequence
        return this.extensions.find((ext) => ext.subs[0].toOID() === oid);
    }
    // A certificate should be considered invalid if it contains critical
    // extensions that are not recognized
    checkRecognizedExtensions() {
        // The extension list is the first (and only) element of the extensions
        // context specific tag
        const extSeq = this.extensionsObj?.subs[0];
        const exts = extSeq?.subs.map((ext) => new ext_1.x509Extension(ext));
        // Check for unrecognized critical extensions
        return (!exts ||
            exts.every((ext) => !ext.critical || RECOGNIZED_EXTENSIONS.includes(ext.oid)));
    }
    /////////////////////////////////////////////////////////////////////////////
    // The following properties use the documented x509 structure to locate the
    // desired ASN.1 object
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.1.1
    get tbsCertificateObj() {
        // tbsCertificate is the first element of the certificate sequence
        return this.root.subs[0];
    }
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.1.2
    get signatureAlgorithmObj() {
        // signatureAlgorithm is the second element of the certificate sequence
        return this.root.subs[1];
    }
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.1.3
    get signatureValueObj() {
        // signatureValue is the third element of the certificate sequence
        return this.root.subs[2];
    }
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.1
    get versionObj() {
        // version is the first element of the tbsCertificate sequence
        return this.tbsCertificateObj.subs[0];
    }
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.4
    get issuerObj() {
        // issuer is the fourth element of the tbsCertificate sequence
        return this.tbsCertificateObj.subs[3];
    }
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.5
    get validityObj() {
        // version is the fifth element of the tbsCertificate sequence
        return this.tbsCertificateObj.subs[4];
    }
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.6
    get subjectObj() {
        // subject is the sixth element of the tbsCertificate sequence
        return this.tbsCertificateObj.subs[5];
    }
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.7
    get subjectPublicKeyInfoObj() {
        // subjectPublicKeyInfo is the seventh element of the tbsCertificate sequence
        return this.tbsCertificateObj.subs[6];
    }
    // Extensions can't be located by index because their position varies. Instead,
    // we need to find the extensions context specific tag
    // https://www.rfc-editor.org/rfc/rfc5280#section-4.1.2.9
    get extensionsObj() {
        return this.tbsCertificateObj.subs.find((sub) => sub.tag.isContextSpecific(0x03));
    }
}
exports.x509Certificate = x509Certificate;
