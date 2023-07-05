"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.OAuthProvider = void 0;
/*
Copyright 2022 The Sigstore Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
const assert_1 = __importDefault(require("assert"));
const child_process_1 = __importDefault(require("child_process"));
const http_1 = __importDefault(require("http"));
const make_fetch_happen_1 = __importDefault(require("make-fetch-happen"));
const url_1 = require("url");
const util_1 = require("../util");
class OAuthProvider {
    constructor(options) {
        this.clientID = options.clientID;
        this.clientSecret = options.clientSecret || '';
        this.issuer = options.issuer;
        this.redirectURI = options.redirectURL;
        this.codeVerifier = generateRandomString(32);
        this.state = generateRandomString(16);
    }
    async getToken() {
        const authCode = await this.initiateAuthRequest();
        return this.getIDToken(authCode);
    }
    // Initates the authorization request. This will start an HTTP server to
    // receive the post-auth redirect and then open the user's default browser to
    // the provider's authorization page.
    async initiateAuthRequest() {
        const server = http_1.default.createServer();
        const sockets = new Set();
        // Start server and wait till it is listening. If a redirect URL was
        // provided, use that. Otherwise, use a random port and construct the
        // redirect URL.
        await new Promise((resolve) => {
            if (this.redirectURI) {
                const url = new url_1.URL(this.redirectURI);
                server.listen(Number(url.port), url.hostname, resolve);
            }
            else {
                server.listen(0, resolve);
                // Get port the server is listening on and construct the server URL
                const port = server.address().port;
                this.redirectURI = `http://localhost:${port}`;
            }
        });
        // Keep track of connections to the server so we can force a shutdown
        server.on('connection', (socket) => {
            sockets.add(socket);
            socket.once('close', () => {
                sockets.delete(socket);
            });
        });
        const result = new Promise((resolve, reject) => {
            // Set-up handler for post-auth redirect
            server.on('request', (req, res) => {
                if (!req.url) {
                    reject('invalid server request');
                    return;
                }
                res.writeHead(200);
                res.end('Auth Successful');
                // Parse incoming request URL
                const query = new url_1.URL(req.url, this.redirectURI).searchParams;
                // Check to see if the state matches
                if (query.get('state') !== this.state) {
                    reject('invalid state value');
                    return;
                }
                const authCode = query.get('code');
                // Force-close any open connections to the server so we can get a
                // clean shutdown
                for (const socket of sockets) {
                    socket.destroy();
                    sockets.delete(socket);
                }
                // Return auth code once we've shutdown server
                server.close(() => {
                    if (!authCode) {
                        reject('authorization code not found');
                    }
                    else {
                        resolve(authCode);
                    }
                });
            });
        });
        try {
            // Open browser to start authorization request
            const authBaseURL = await this.issuer.authEndpoint();
            const authURL = this.getAuthRequestURL(authBaseURL);
            await this.openURL(authURL);
        }
        catch (err) {
            // Prevent leaked server handler on error
            server.close();
            throw err;
        }
        return result;
    }
    // Uses the provided authorization code, to retrieve the ID token from the
    // provider
    async getIDToken(authCode) {
        (0, assert_1.default)(this.redirectURI);
        const tokenEndpointURL = await this.issuer.tokenEndpoint();
        const params = new url_1.URLSearchParams();
        params.append('grant_type', 'authorization_code');
        params.append('code', authCode);
        params.append('redirect_uri', this.redirectURI);
        params.append('code_verifier', this.codeVerifier);
        const response = await (0, make_fetch_happen_1.default)(tokenEndpointURL, {
            method: 'POST',
            headers: { Authorization: `Basic ${this.getBasicAuthHeaderValue()}` },
            body: params,
        }).then((r) => r.json());
        return response.id_token;
    }
    // Construct the basic auth header value from the client ID and secret
    getBasicAuthHeaderValue() {
        return util_1.encoding.base64Encode(`${this.clientID}:${this.clientSecret}`);
    }
    // Generate starting URL for authorization request
    getAuthRequestURL(baseURL) {
        const params = this.getAuthRequestParams();
        return `${baseURL}?${params.toString()}`;
    }
    // Collect parameters for authorization request
    getAuthRequestParams() {
        (0, assert_1.default)(this.redirectURI);
        const codeChallenge = this.getCodeChallenge();
        return new url_1.URLSearchParams({
            response_type: 'code',
            client_id: this.clientID,
            client_secret: this.clientSecret,
            scope: 'openid email',
            redirect_uri: this.redirectURI,
            code_challenge: codeChallenge,
            code_challenge_method: 'S256',
            state: this.state,
            nonce: generateRandomString(16),
        });
    }
    // Generate code challenge for authorization request
    getCodeChallenge() {
        return util_1.encoding.base64URLEscape(util_1.crypto.hash(this.codeVerifier).toString('base64'));
    }
    // Open the supplied URL in the user's default browser
    async openURL(url) {
        return new Promise((resolve, reject) => {
            let open = null;
            let command = `"${url}"`;
            switch (process.platform) {
                case 'darwin':
                    open = 'open';
                    break;
                case 'linux' || 'freebsd' || 'netbsd' || 'openbsd':
                    open = 'xdg-open';
                    break;
                case 'win32':
                    open = 'start';
                    command = `"" ${command}`;
                    break;
                default:
                    return reject(`OAuth: unsupported platform: ${process.platform}`);
            }
            console.error(`Your browser will now be opened to: ${url}`);
            child_process_1.default.exec(`${open} ${command}`, undefined, (err) => {
                if (err) {
                    reject(err);
                }
                else {
                    resolve();
                }
            });
        });
    }
}
exports.OAuthProvider = OAuthProvider;
// Generate random code verifier value
function generateRandomString(len) {
    return util_1.encoding.base64URLEscape(util_1.crypto.randomBytes(len).toString('base64'));
}
