/// <reference types="node" />
/// <reference types="node" />
import { KeyObject } from 'crypto';
import { CertificateRequest } from '../client/fulcio';
export declare function toCertificateRequest(publicKey: KeyObject, challenge: Buffer): CertificateRequest;
