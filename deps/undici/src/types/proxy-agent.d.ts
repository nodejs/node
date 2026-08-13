import Agent from './agent'
import buildConnector from './connector'
import Dispatcher from './dispatcher'
import { OutgoingHttpHeaders } from './header'

export default ProxyAgent

declare class ProxyAgent extends Dispatcher {
  constructor (options: ProxyAgent.Options | string)

  dispatch (options: Agent.DispatchOptions, handler: Dispatcher.DispatchHandler): boolean
  close (): Promise<void>
}

declare namespace ProxyAgent {
  export interface Options extends Agent.Options {
    uri: string;
    /**
     * @deprecated use opts.token
     */
    auth?: string;
    token?: string;
    headers?: OutgoingHttpHeaders;
    requestTls?: buildConnector.BuildOptions;
    proxyTls?: buildConnector.BuildOptions;
    clientFactory?(origin: URL, opts: object): Dispatcher;
    /**
     * Undici tunnels via CONNECT when the target endpoint uses HTTPS, and
     * forwards (HTTP/1.1 absolute-form request-target, per RFC 9112 §3.2.2)
     * otherwise, regardless of whether the proxy URL is HTTP or HTTPS. The
     * non-tunneled forwarding path does not negotiate HTTP/2 with the proxy.
     * Set to true to force tunneling for plain HTTP requests as well.
     */
    proxyTunnel?: boolean;
  }
}
