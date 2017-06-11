
/**
 * Module dependencies.
 */

var tls; // lazy-loaded...
var url = require('url');
var dns = require('dns');
var extend = require('extend');
var Agent = require('agent-base');
var SocksClient = require('socks');
var inherits = require('util').inherits;

/**
 * Module exports.
 */

module.exports = SocksProxyAgent;

/**
 * The `SocksProxyAgent`.
 *
 * @api public
 */

function SocksProxyAgent (opts) {
  if (!(this instanceof SocksProxyAgent)) return new SocksProxyAgent(opts);
  if ('string' == typeof opts) opts = url.parse(opts);
  if (!opts) throw new Error('a SOCKS proxy server `host` and `port` must be specified!');
  Agent.call(this, connect);

  var proxy = extend({}, opts);

  // prefer `hostname` over `host`, because of `url.parse()`
  proxy.host = proxy.hostname || proxy.host;

  // SOCKS doesn't *technically* have a default port, but this is
  // the same default that `curl(1)` uses
  proxy.port = +proxy.port || 1080;

  if (proxy.host && proxy.path) {
    // if both a `host` and `path` are specified then it's most likely the
    // result of a `url.parse()` call... we need to remove the `path` portion so
    // that `net.connect()` doesn't attempt to open that as a unix socket file.
    delete proxy.path;
    delete proxy.pathname;
  }

  // figure out if we want socks v4 or v5, based on the "protocol" used.
  // Defaults to 5.
  proxy.lookup = false;
  switch (proxy.protocol) {
    case 'socks4:':
      proxy.lookup = true;
      // pass through
    case 'socks4a:':
      proxy.version = 4;
      break;
    case 'socks5:':
      proxy.lookup = true;
      // pass through
    case 'socks:': // no version specified, default to 5h
    case 'socks5h:':
      proxy.version = 5;
      break;
    default:
      throw new TypeError('A "socks" protocol must be specified! Got: ' + proxy.protocol);
  }

  if (proxy.auth) {
    var auth = proxy.auth.split(':');
    proxy.authentication = {username: auth[0], password: auth[1]};
    proxy.userid = auth[0];
  }
  this.proxy = proxy;
}
inherits(SocksProxyAgent, Agent);

/**
 * Initiates a SOCKS connection to the specified SOCKS proxy server,
 * which in turn connects to the specified remote host and port.
 *
 * @api public
 */

function connect (req, opts, fn) {

  var proxy = this.proxy;

  // called once the SOCKS proxy has connected to the specified remote endpoint
  function onhostconnect (err, socket) {
    if (err) return fn(err);
    var s = socket;
    if (opts.secureEndpoint) {
      // since the proxy is connecting to an SSL server, we have
      // to upgrade this socket connection to an SSL connection
      if (!tls) tls = require('tls');
      opts.socket = socket;
      opts.servername = opts.host;
      opts.host = null;
      opts.hostname = null;
      opts.port = null;
      s = tls.connect(opts);
      socket.resume();
    }
    fn(null, s);
  }

  // called for the `dns.lookup()` callback
  function onlookup (err, ip) {
    if (err) return fn(err);
    options.target.host = ip;
    SocksClient.createConnection(options, onhostconnect);
  }

  var options = {
    proxy: {
      ipaddress: proxy.host,
      port: +proxy.port,
      type: proxy.version
    },
    target: {
      port: +opts.port
    },
    command: 'connect'
  };
  if (proxy.authentication) {
    options.proxy.authentication = proxy.authentication;
    options.proxy.userid = proxy.userid;
  }

  if (proxy.lookup) {
    // client-side DNS resolution for "4" and "5" socks proxy versions
    dns.lookup(opts.host, onlookup);
  } else {
    // proxy hostname DNS resolution for "4a" and "5h" socks proxy servers
    onlookup(null, opts.host)
  }
}
