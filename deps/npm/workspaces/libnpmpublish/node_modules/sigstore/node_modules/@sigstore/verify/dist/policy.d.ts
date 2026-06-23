import { CertificateExtensions } from './shared.types';
import type { ObjectIdentifierValuePair } from '@sigstore/protobuf-specs';
export declare function verifySubjectAlternativeName(policyIdentity: string, signerIdentity: string | undefined): void;
export declare function verifyExtensions(policyExtensions: CertificateExtensions, signerExtensions?: CertificateExtensions): void;
export declare function verifyOIDs(policyOIDs: ObjectIdentifierValuePair[], signerOIDs?: ObjectIdentifierValuePair[]): void;
