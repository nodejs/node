/// <reference types="node" />
import { Url } from 'url';
import { SocksProxy } from 'socks';
import tls from 'tls';
import { AgentOptions } from 'agent-base';
import _SocksProxyAgent from './agent';
declare function createSocksProxyAgent(opts: string | createSocksProxyAgent.SocksProxyAgentOptions): _SocksProxyAgent;
declare namespace createSocksProxyAgent {
    interface BaseSocksProxyAgentOptions {
        host?: string | null;
        port?: string | number | null;
        username?: string | null;
        tls?: tls.ConnectionOptions | null;
    }
    export interface SocksProxyAgentOptions extends AgentOptions, BaseSocksProxyAgentOptions, Partial<Omit<Url & SocksProxy, keyof BaseSocksProxyAgentOptions>> {
    }
    export type SocksProxyAgent = _SocksProxyAgent;
    export const SocksProxyAgent: typeof _SocksProxyAgent;
    export {};
}
export = createSocksProxyAgent;
