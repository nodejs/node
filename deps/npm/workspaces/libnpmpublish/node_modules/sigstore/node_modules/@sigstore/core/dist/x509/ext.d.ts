import { ASN1Obj } from '../asn1';
import { SignedCertificateTimestamp } from './sct';
export declare class X509Extension {
    protected root: ASN1Obj;
    constructor(asn1: ASN1Obj);
    get oid(): string;
    get critical(): boolean;
    get value(): Buffer<ArrayBufferLike>;
    get valueObj(): ASN1Obj;
    protected get extnValueObj(): ASN1Obj;
}
export declare class X509BasicConstraintsExtension extends X509Extension {
    get isCA(): boolean;
    get pathLenConstraint(): bigint | undefined;
    private get sequence();
}
export declare class X509KeyUsageExtension extends X509Extension {
    get digitalSignature(): boolean;
    get keyCertSign(): boolean;
    get crlSign(): boolean;
    private get bitString();
}
export declare class X509SubjectAlternativeNameExtension extends X509Extension {
    get rfc822Name(): string | undefined;
    get uri(): string | undefined;
    otherName(oid: string): string | undefined;
    private findGeneralName;
    private get generalNames();
}
export declare class X509AuthorityKeyIDExtension extends X509Extension {
    get keyIdentifier(): Buffer | undefined;
    private findSequenceMember;
    private get sequence();
}
export declare class X509SubjectKeyIDExtension extends X509Extension {
    get keyIdentifier(): Buffer;
}
export declare class X509SCTExtension extends X509Extension {
    constructor(asn1: ASN1Obj);
    get signedCertificateTimestamps(): SignedCertificateTimestamp[];
}
