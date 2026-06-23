export declare class HTTPError extends Error {
    statusCode: number;
    location?: string;
    constructor({ status, message, location, }: {
        status: number;
        message: string;
        location?: string;
    });
}
