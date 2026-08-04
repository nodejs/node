import { SocksClient } from 'socks';
import { Agent } from 'agent-base';
import createDebug from 'debug';
import * as dns from 'dns';
import * as net from 'net';
import * as tls from 'tls';
import { URL } from 'url';
const debug = createDebug('socks-proxy-agent');
const setServernameFromNonIpHost = (options) => {
    if (options.servername === undefined &&
        options.host &&
        !net.isIP(options.host)) {
        return {
            ...options,
            servername: options.host,
        };
    }
    return options;
};
function parseSocksURL(url) {
    let lookup = false;
    let type = 5;
    const host = url.hostname;
    // From RFC 1928, Section 3: https://tools.ietf.org/html/rfc1928#section-3
    // "The SOCKS service is conventionally located on TCP port 1080"
    const port = parseInt(url.port, 10) || 1080;
    // figure out if we want socks v4 or v5, based on the "protocol" used.
    // Defaults to 5.
    switch (url.protocol.replace(':', '')) {
        case 'socks4':
            lookup = true;
            type = 4;
            break;
        // pass through
        case 'socks4a':
            type = 4;
            break;
        case 'socks5':
            lookup = true;
            type = 5;
            break;
        // pass through
        case 'socks': // no version specified, default to 5h
            type = 5;
            break;
        case 'socks5h':
            type = 5;
            break;
        default:
            throw new TypeError(`A "socks" protocol must be specified! Got: ${String(url.protocol)}`);
    }
    const proxy = {
        host,
        port,
        type,
    };
    if (url.username) {
        Object.defineProperty(proxy, 'userId', {
            value: decodeURIComponent(url.username),
            enumerable: false,
        });
    }
    if (url.password != null) {
        Object.defineProperty(proxy, 'password', {
            value: decodeURIComponent(url.password),
            enumerable: false,
        });
    }
    return { lookup, proxy };
}
export class SocksProxyAgent extends Agent {
    constructor(uri, opts) {
        super(opts);
        const url = typeof uri === 'string' ? new URL(uri) : uri;
        const { proxy, lookup } = parseSocksURL(url);
        this.shouldLookup = lookup;
        this.proxy = proxy;
        this.proxyUrl = url.href;
        this.timeout = opts?.timeout ?? null;
        this.socketOptions = opts?.socketOptions ?? null;
    }
    /**
     * Initiates a SOCKS connection to the specified SOCKS proxy server,
     * which in turn connects to the specified remote host and port.
     */
    async connect(req, opts) {
        const { shouldLookup, proxy, timeout } = this;
        if (!opts.host) {
            throw new Error('No `host` defined!');
        }
        let { host } = opts;
        const { port, lookup: lookupFn = dns.lookup } = opts;
        if (shouldLookup) {
            // Client-side DNS resolution for "4" and "5" socks proxy versions.
            host = await new Promise((resolve, reject) => {
                // Use the request's custom lookup, if one was configured:
                lookupFn(host, {}, (err, address) => {
                    if (err) {
                        reject(err);
                    }
                    else {
                        resolve(typeof address === 'string'
                            ? address
                            : address[0].address);
                    }
                });
            });
        }
        const socksOpts = {
            proxy,
            destination: {
                host,
                port: typeof port === 'number' ? port : parseInt(port, 10),
            },
            command: 'connect',
            timeout: timeout ?? undefined,
            // @ts-expect-error the type supplied by socks for socket_options is wider
            // than necessary since socks will always override the host and port
            socket_options: this.socketOptions ?? undefined,
        };
        const cleanup = (tlsSocket) => {
            req.destroy();
            socket.destroy();
            if (tlsSocket)
                tlsSocket.destroy();
        };
        debug('Creating socks proxy connection: %o', socksOpts);
        const { socket } = await SocksClient.createConnection(socksOpts);
        debug('Successfully created socks proxy connection');
        req.emit('proxy', { proxy: this.proxyUrl, socket });
        if (timeout !== null) {
            socket.setTimeout(timeout);
            socket.on('timeout', () => cleanup());
        }
        if (opts.secureEndpoint) {
            // The proxy is connecting to a TLS server, so upgrade
            // this socket connection to a TLS connection.
            debug('Upgrading socket connection to TLS');
            const tlsSocket = tls.connect({
                ...omit(setServernameFromNonIpHost(opts), 'host', 'path', 'port'),
                socket,
            });
            tlsSocket.once('error', (error) => {
                debug('Socket TLS error', error.message);
                cleanup(tlsSocket);
            });
            return tlsSocket;
        }
        return socket;
    }
}
SocksProxyAgent.protocols = [
    'socks',
    'socks4',
    'socks4a',
    'socks5',
    'socks5h',
];
function omit(obj, ...keys) {
    const ret = {};
    let key;
    for (key in obj) {
        if (!keys.includes(key)) {
            ret[key] = obj[key];
        }
    }
    return ret;
}
//# sourceMappingURL=index.js.map