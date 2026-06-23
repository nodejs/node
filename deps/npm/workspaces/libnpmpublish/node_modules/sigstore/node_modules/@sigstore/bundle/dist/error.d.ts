export declare class ValidationError extends Error {
    fields: string[];
    constructor(message: string, fields: string[]);
}
