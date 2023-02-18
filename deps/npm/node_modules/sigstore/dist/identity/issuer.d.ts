/**
 * The Issuer reperesents a single OAuth2 provider.
 *
 * The Issuer is configured with a provider's base OAuth2 endpoint which is
 * used to retrieve the associated configuration information.
 */
export declare class Issuer {
    private baseURL;
    private fetch;
    private config?;
    constructor(baseURL: string);
    authEndpoint(): Promise<string>;
    tokenEndpoint(): Promise<string>;
    private loadOpenIDConfig;
}
