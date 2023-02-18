import { Issuer } from './issuer';
import { Provider } from './provider';
export declare class OAuthProvider implements Provider {
    private clientID;
    private clientSecret;
    private issuer;
    private codeVerifier;
    private state;
    private redirectURI?;
    constructor(issuer: Issuer, clientID: string, clientSecret?: string);
    getToken(): Promise<string>;
    private initiateAuthRequest;
    private getIDToken;
    private getBasicAuthHeaderValue;
    private getAuthRequestURL;
    private getAuthRequestParams;
    private getCodeChallenge;
    private openURL;
}
