export interface Provider {
    getToken: () => Promise<string>;
}
