export interface IdentityProvider {
    getToken: () => Promise<string>;
}
