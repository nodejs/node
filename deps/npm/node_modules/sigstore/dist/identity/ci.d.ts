import { Provider } from './provider';
/**
 * CIContextProvider is a composite identity provider which will iterate
 * over all of the CI-specific providers and return the token from the first
 * one that resolves.
 */
export declare class CIContextProvider implements Provider {
    private audience;
    constructor(audience: string);
    getToken(): Promise<string>;
}
