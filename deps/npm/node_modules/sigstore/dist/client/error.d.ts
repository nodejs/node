import fetch from 'make-fetch-happen';
type Response = Awaited<ReturnType<typeof fetch>>;
export declare class HTTPError extends Error {
    response: Response;
    statusCode: number;
    location?: string;
    constructor(response: Response);
}
export declare const checkStatus: (response: Response) => Response;
export {};
