import { SocksClientOptions, SocksClientChainOptions } from './constants';
/**
 * Error wrapper for SocksClient
 */
declare class SocksClientError extends Error {
    options: SocksClientOptions | SocksClientChainOptions;
    constructor(message: string, options: SocksClientOptions | SocksClientChainOptions);
}
/**
 * Shuffles a given array.
 * @param array The array to shuffle.
 */
declare function shuffleArray(array: any[]): void;
declare type RequireOnlyOne<T, Keys extends keyof T = keyof T> = Pick<T, Exclude<keyof T, Keys>> & {
    [K in Keys]?: Required<Pick<T, K>> & Partial<Record<Exclude<Keys, K>, undefined>>;
}[Keys];
export { RequireOnlyOne, SocksClientError, shuffleArray };
