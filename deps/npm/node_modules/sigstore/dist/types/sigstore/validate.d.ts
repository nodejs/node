import { WithRequired } from '../utility';
import { Bundle, VerificationMaterial } from './__generated__/sigstore_bundle';
import { MessageSignature } from './__generated__/sigstore_common';
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
