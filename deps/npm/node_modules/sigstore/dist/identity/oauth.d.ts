import { Issuer } from './issuer';
import { Provider } from './provider';
interface OAuthProviderOptions {
    issuer: Issuer;
    clientID: string;
    clientSecret?: string;
    redirectURL?: string;
}
export declare class OAuthProvider implements Provider {
    private clientID;
    private clientSecret;
    private issuer;
    private codeVerifier;
    private state;
    private redirectURI?;
    constructor(options: OAuthProviderOptions);
    getToken(): Promise<string>;
    private initiateAuthRequest;
    private getIDToken;
    private getBasicAuthHeaderValue;
    private getAuthRequestURL;
    private getAuthRequestParams;
    private getCodeChallenge;
    private openURL;
}
export {};
