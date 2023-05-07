/// <reference types="node" />
import { ASN1Obj } from './asn1/obj';
import { SignedCertificateTimestamp } from './sct';
export declare class x509Extension {
    protected root: ASN1Obj;
    constructor(asn1: ASN1Obj);
    get oid(): string;
    get critical(): boolean;
    get value(): Buffer;
    get valueObj(): ASN1Obj;
    protected get extnValueObj(): ASN1Obj;
}
export declare class x509BasicConstraintsExtension extends x509Extension {
    get isCA(): boolean;
    get pathLenConstraint(): bigint | undefined;
    private get sequence();
}
export declare class x509KeyUsageExtension extends x509Extension {
    get digitalSignature(): boolean;
    get keyCertSign(): boolean;
    get crlSign(): boolean;
    private get bitString();
}
export declare class x509SubjectAlternativeNameExtension extends x509Extension {
    get rfc822Name(): string | undefined;
    get uri(): string | undefined;
    otherName(oid: string): string | undefined;
    private findGeneralName;
    private get generalNames();
}
export declare class x509AuthorityKeyIDExtension extends x509Extension {
    get keyIdentifier(): Buffer | undefined;
    private findSequenceMember;
    private get sequence();
}
export declare class x509SubjectKeyIDExtension extends x509Extension {
    get keyIdentifier(): Buffer;
}
export declare class x509SCTExtension extends x509Extension {
    constructor(asn1: ASN1Obj);
    get signedCertificateTimestamps(): SignedCertificateTimestamp[];
}
