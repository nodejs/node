import { Bundle } from '@sigstore/bundle';
import type { SignatureContent, SignedEntity } from '../shared.types';
export declare function toSignedEntity(bundle: Bundle, artifact?: Buffer): SignedEntity;
export declare function signatureContent(bundle: Bundle, artifact?: Buffer): SignatureContent;
