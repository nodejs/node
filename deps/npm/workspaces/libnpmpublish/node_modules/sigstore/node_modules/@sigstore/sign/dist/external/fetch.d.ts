import fetch, { FetchOptions } from 'make-fetch-happen';
type Response = Awaited<ReturnType<typeof fetch>>;
export declare function fetchWithRetry(url: string, options: FetchOptions): Promise<Response>;
export {};
