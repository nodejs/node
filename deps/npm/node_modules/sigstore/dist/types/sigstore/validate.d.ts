import { Bundle, MessageSignature, VerificationMaterial } from '@sigstore/protobuf-specs';
import { WithRequired } from '../utility';
export type ValidBundle = Bundle & {
    verificationMaterial: VerificationMaterial & {
        content: NonNullable<VerificationMaterial['content']>;
    };
    content: (Extract<Bundle['content'], {
        $case: 'messageSignature';
    }> & {
        messageSignature: WithRequired<MessageSignature, 'messageDigest'>;
    }) | Extract<Bundle['content'], {
        $case: 'dsseEnvelope';
    }>;
};
export declare function assertValidBundle(b: Bundle): asserts b is ValidBundle;
