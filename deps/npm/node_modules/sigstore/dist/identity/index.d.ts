import { Provider } from './provider';
/**
 * oauthProvider returns a new Provider instance which attempts to retrieve
 * an identity token from the configured OAuth2 issuer.
 *
 * @param issuer Base URL of the issuer
 * @param clientID Client ID for the issuer
 * @param clientSecret Client secret for the issuer (optional)
 * @returns {Provider}
 */
declare function oauthProvider(options: {
    issuer: string;
    clientID: string;
    clientSecret?: string;
    redirectURL?: string;
}): Provider;
/**
 * ciContextProvider returns a new Provider instance which attempts to retrieve
 * an identity token from the CI context.
 *
 * @param audience audience claim for the generated token
 * @returns {Provider}
 */
declare function ciContextProvider(audience?: string): Provider;
declare const _default: {
    ciContextProvider: typeof ciContextProvider;
    oauthProvider: typeof oauthProvider;
};
export default _default;
export { Provider } from './provider';
