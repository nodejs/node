/// <reference types="node" />
import * as sigstore from '../types/sigstore';
import { ASN1Obj } from '../util/asn1';
import { x509AuthorityKeyIDExtension, x509BasicConstraintsExtension, x509Extension, x509KeyUsageExtension, x509SCTExtension, x509SubjectAlternativeNameExtension, x509SubjectKeyIDExtension } from './ext';
interface SCTVerificationResult {
    verified: boolean;
    logID: Buffer;
}
export declare class x509Certificate {
    root: ASN1Obj;
    constructor(asn1: ASN1Obj);
    static parse(cert: Buffer | string): x509Certificate;
    get tbsCertificate(): ASN1Obj;
    get version(): string;
    get notBefore(): Date;
    get notAfter(): Date;
    get issuer(): Buffer;
    get subject(): Buffer;
    get publicKey(): Buffer;
    get signatureAlgorithm(): string;
    get signatureValue(): Buffer;
    get extensions(): ASN1Obj[];
    get extKeyUsage(): x509KeyUsageExtension | undefined;
    get extBasicConstraints(): x509BasicConstraintsExtension | undefined;
    get extSubjectAltName(): x509SubjectAlternativeNameExtension | undefined;
    get extAuthorityKeyID(): x509AuthorityKeyIDExtension | undefined;
    get extSubjectKeyID(): x509SubjectKeyIDExtension | undefined;
    get extSCT(): x509SCTExtension | undefined;
    get isCA(): boolean;
    extension(oid: string): x509Extension | undefined;
    verify(issuerCertificate?: x509Certificate): boolean;
    validForDate(date: Date): boolean;
    equals(other: x509Certificate): boolean;
    verifySCTs(issuer: x509Certificate, logs: sigstore.TransparencyLogInstance[]): SCTVerificationResult[];
    private clone;
    private findExtension;
    private checkRecognizedExtensions;
    private get tbsCertificateObj();
    private get signatureAlgorithmObj();
    private get signatureValueObj();
    private get versionObj();
    private get issuerObj();
    private get validityObj();
    private get subjectObj();
    private get subjectPublicKeyInfoObj();
    private get extensionsObj();
}
export {};
