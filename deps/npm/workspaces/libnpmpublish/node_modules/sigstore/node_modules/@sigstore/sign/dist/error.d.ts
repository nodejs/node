type InternalErrorCode = 'TLOG_FETCH_ENTRY_ERROR' | 'TLOG_CREATE_ENTRY_ERROR' | 'CA_CREATE_SIGNING_CERTIFICATE_ERROR' | 'TSA_CREATE_TIMESTAMP_ERROR' | 'IDENTITY_TOKEN_READ_ERROR' | 'IDENTITY_TOKEN_PARSE_ERROR';
export declare class InternalError extends Error {
    code: InternalErrorCode;
    cause: any | undefined;
    constructor({ code, message, cause, }: {
        code: InternalErrorCode;
        message: string;
        cause?: any;
    });
}
export declare function internalError(err: unknown, code: InternalErrorCode, message: string): never;
export {};
