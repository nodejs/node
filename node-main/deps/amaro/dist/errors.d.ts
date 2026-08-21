type SwcError = {
    code: "UnsupportedSyntax" | "InvalidSyntax";
    message: string;
    startColumn: number;
    startLine: number;
    snippet: string;
    filename: string;
    endColumn: number;
    endLine: number;
};
export declare function isSwcError(error: unknown): error is SwcError;
export declare function wrapAndReThrowSwcError(error: SwcError): never;
export {};
