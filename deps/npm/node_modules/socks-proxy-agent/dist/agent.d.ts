/// <reference types="node" />
import net from 'net';
import { Agent, ClientRequest, RequestOptions } from 'agent-base';
import { SocksProxyAgentOptions } from '.';
/**
 * The `SocksProxyAgent`.
 *
 * @api public
 */
export default class SocksProxyAgent extends Agent {
    private lookup;
    private proxy;
    constructor(_opts: string | SocksProxyAgentOptions);
    /**
     * Initiates a SOCKS connection to the specified SOCKS proxy server,
     * which in turn connects to the specified remote host and port.
     *
     * @api protected
     */
    callback(req: ClientRequest, opts: RequestOptions): Promise<net.Socket>;
}
