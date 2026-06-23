import * as net from 'net';
import * as tls from 'tls';
import * as http from 'http';
import { Agent, AgentConnectOpts } from 'agent-base';
import { URL } from 'url';
import type { OutgoingHttpHeaders } from 'http';
import { type OnProxyAuthCallback, type ProxyAuthParams, type ProxyAuthResponse } from 'proxy-agent-negotiate';
export type { OnProxyAuthCallback, ProxyAuthParams, ProxyAuthResponse };
export interface ProxyConnect {
    socket: net.Socket;
}
type Protocol<T> = T extends `${infer Protocol}:${infer _}` ? Protocol : never;
type ConnectOptsMap = {
    http: Omit<net.TcpNetConnectOpts, 'host' | 'port'>;
    https: Omit<tls.ConnectionOptions, 'host' | 'port'>;
};
type ConnectOpts<T> = {
    [P in keyof ConnectOptsMap]: Protocol<T> extends P ? ConnectOptsMap[P] : never;
}[keyof ConnectOptsMap];
export type HttpProxyAgentOptions<T> = ConnectOpts<T> & http.AgentOptions & {
    headers?: OutgoingHttpHeaders | (() => OutgoingHttpHeaders);
    onProxyAuth?: OnProxyAuthCallback;
    negotiate?: boolean;
};
interface HttpProxyAgentClientRequest extends http.ClientRequest {
    outputData?: {
        data: string;
    }[];
    _header?: string | null;
    _implicitHeader(): void;
}
/**
 * The `HttpProxyAgent` implements an HTTP Agent subclass that connects
 * to the specified "HTTP proxy server" in order to proxy HTTP requests.
 */
export declare class HttpProxyAgent<Uri extends string> extends Agent {
    static protocols: readonly ["http", "https"];
    readonly proxy: URL;
    proxyHeaders: OutgoingHttpHeaders | (() => OutgoingHttpHeaders);
    connectOpts: net.TcpNetConnectOpts & tls.ConnectionOptions;
    onProxyAuth?: OnProxyAuthCallback;
    constructor(proxy: Uri | URL, opts?: HttpProxyAgentOptions<Uri>);
    addRequest(req: HttpProxyAgentClientRequest, opts: AgentConnectOpts): void;
    setRequestProps(req: HttpProxyAgentClientRequest, opts: AgentConnectOpts): void;
    connect(req: HttpProxyAgentClientRequest, opts: AgentConnectOpts): Promise<net.Socket>;
}
//# sourceMappingURL=index.d.ts.map