/// <reference types="node" />
import { SignatureMaterial } from '../types/signature';
import { Envelope } from '../types/sigstore';
import { HashedRekordKind, IntotoKind } from './types';
export declare function toProposedHashedRekordEntry(digest: Buffer, signature: SignatureMaterial): HashedRekordKind;
export declare function toProposedIntotoEntry(envelope: Envelope, signature: SignatureMaterial, apiVersion?: string): IntotoKind;
