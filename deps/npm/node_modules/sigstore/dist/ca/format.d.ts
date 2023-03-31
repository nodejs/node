/// <reference types="node" />
/// <reference types="node" />
import { KeyObject } from 'crypto';
import { SigningCertificateRequest } from '../client/fulcio';
export declare function toCertificateRequest(identityToken: string, publicKey: KeyObject, challenge: Buffer): SigningCertificateRequest;
