declare class BaseError extends Error {
    cause: any | undefined;
    constructor(message: string, cause?: any);
}
export declare class VerificationError extends BaseError {
}
export declare class ValidationError extends BaseError {
}
export declare class InternalError extends BaseError {
}
export declare class PolicyError extends BaseError {
}
export {};
