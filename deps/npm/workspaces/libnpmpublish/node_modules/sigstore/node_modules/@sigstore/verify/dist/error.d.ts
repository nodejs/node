declare class BaseError<T extends string> extends Error {
    code: T;
    cause: any;
    constructor({ code, message, cause, }: {
        code: T;
        message: string;
        cause?: any;
    });
}
type VerificationErrorCode = 'NOT_IMPLEMENTED_ERROR' | 'TLOG_ERROR' | 'TLOG_INCLUSION_PROOF_ERROR' | 'TLOG_INCLUSION_PROMISE_ERROR' | 'TLOG_MISSING_INCLUSION_ERROR' | 'TLOG_BODY_ERROR' | 'CERTIFICATE_ERROR' | 'PUBLIC_KEY_ERROR' | 'SIGNATURE_ERROR' | 'TIMESTAMP_ERROR';
export declare class VerificationError extends BaseError<VerificationErrorCode> {
}
type PolicyErrorCode = 'UNTRUSTED_SIGNER_ERROR';
export declare class PolicyError extends BaseError<PolicyErrorCode> {
}
export {};
