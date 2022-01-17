"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const dns_1 = __importDefault(require("dns"));
const tls_1 = __importDefault(require("tls"));
const url_1 = __importDefault(require("url"));
const debug_1 = __importDefault(require("debug"));
const agent_base_1 = require("agent-base");
const socks_1 = require("socks");
const debug = debug_1.default('socks-proxy-agent');
function dnsLookup(host) {
    return new Promise((resolve, reject) => {
        dns_1.default.lookup(host, (err, res) => {
            if (err) {
                reject(err);
            }
            else {
                resolve(res);
            }
        });
    });
}
function parseSocksProxy(opts) {
    let port = 0;
    let lookup = false;
    let type = 5;
    // Prefer `hostname` over `host`, because of `url.parse()`
    const host = opts.hostname || opts.host;
    if (!host) {
        throw new TypeError('No "host"');
    }
    if (typeof opts.port === 'number') {
        port = opts.port;
    }
    else if (typeof opts.port === 'string') {
        port = parseInt(opts.port, 10);
    }
    // From RFC 1928, Section 3: https://tools.ietf.org/html/rfc1928#section-3
    // "The SOCKS service is conventionally located on TCP port 1080"
    if (!port) {
        port = 1080;
    }
    // figure out if we want socks v4 or v5, based on the "protocol" used.
    // Defaults to 5.
    if (opts.protocol) {
        switch (opts.protocol.replace(':', '')) {
            case 'socks4':
                lookup = true;
            // pass through
            case 'socks4a':
                type = 4;
                break;
            case 'socks5':
                lookup = true;
            // pass through
            case 'socks': // no version specified, default to 5h
            case 'socks5h':
                type = 5;
                break;
            default:
                throw new TypeError(`A "socks" protocol must be specified! Got: ${opts.protocol}`);
        }
    }
    if (typeof opts.type !== 'undefined') {
        if (opts.type === 4 || opts.type === 5) {
            type = opts.type;
        }
        else {
            throw new TypeError(`"type" must be 4 or 5, got: ${opts.type}`);
        }
    }
    const proxy = {
        host,
        port,
        type
    };
    let userId = opts.userId || opts.username;
    let password = opts.password;
    if (opts.auth) {
        const auth = opts.auth.split(':');
        userId = auth[0];
        password = auth[1];
    }
    if (userId) {
        Object.defineProperty(proxy, 'userId', {
            value: userId,
            enumerable: false
        });
    }
    if (password) {
        Object.defineProperty(proxy, 'password', {
            value: password,
            enumerable: false
        });
    }
    return { lookup, proxy };
}
/**
 * The `SocksProxyAgent`.
 *
 * @api public
 */
class SocksProxyAgent extends agent_base_1.Agent {
    constructor(_opts) {
        let opts;
        if (typeof _opts === 'string') {
            opts = url_1.default.parse(_opts);
        }
        else {
            opts = _opts;
        }
        if (!opts) {
            throw new TypeError('a SOCKS proxy server `host` and `port` must be specified!');
        }
        super(opts);
        const parsedProxy = parseSocksProxy(opts);
        this.lookup = parsedProxy.lookup;
        this.proxy = parsedProxy.proxy;
        this.tlsConnectionOptions = opts.tls || {};
    }
    /**
     * Initiates a SOCKS connection to the specified SOCKS proxy server,
     * which in turn connects to the specified remote host and port.
     *
     * @api protected
     */
    callback(req, opts) {
        return __awaiter(this, void 0, void 0, function* () {
            const { lookup, proxy } = this;
            let { host, port, timeout } = opts;
            if (!host) {
                throw new Error('No `host` defined!');
            }
            if (lookup) {
                // Client-side DNS resolution for "4" and "5" socks proxy versions.
                host = yield dnsLookup(host);
            }
            const socksOpts = {
                proxy,
                destination: { host, port },
                command: 'connect',
                timeout
            };
            debug('Creating socks proxy connection: %o', socksOpts);
            const { socket } = yield socks_1.SocksClient.createConnection(socksOpts);
            debug('Successfully created socks proxy connection');
            if (opts.secureEndpoint) {
                // The proxy is connecting to a TLS server, so upgrade
                // this socket connection to a TLS connection.
                debug('Upgrading socket connection to TLS');
                const servername = opts.servername || opts.host;
                return tls_1.default.connect(Object.assign(Object.assign(Object.assign({}, omit(opts, 'host', 'hostname', 'path', 'port')), { socket,
                    servername }), this.tlsConnectionOptions));
            }
            return socket;
        });
    }
}
exports.default = SocksProxyAgent;
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
//# sourceMappingURL=agent.js.map