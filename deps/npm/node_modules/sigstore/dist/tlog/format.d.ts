/// <reference types="node" />
import { SignatureMaterial } from '../types/signature';
import { Envelope } from '../types/sigstore';
import type { ProposedDSSEEntry, ProposedHashedRekordEntry, ProposedIntotoEntry } from '../external/rekor';
export declare function toProposedDSSEEntry(envelope: Envelope, signature: SignatureMaterial, apiVersion?: string): ProposedDSSEEntry;
export declare function toProposedHashedRekordEntry(digest: Buffer, signature: SignatureMaterial): ProposedHashedRekordEntry;
export declare function toProposedIntotoEntry(envelope: Envelope, signature: SignatureMaterial, apiVersion?: string): ProposedIntotoEntry;
