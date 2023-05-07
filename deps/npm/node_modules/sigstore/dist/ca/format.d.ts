/// <reference types="node" />
/// <reference types="node" />
import { KeyObject } from 'crypto';
import type { SigningCertificateRequest } from '../external/fulcio';
export declare function toCertificateRequest(identityToken: string, publicKey: KeyObject, challenge: Buffer): SigningCertificateRequest;
