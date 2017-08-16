var tls = require('tls');
var inherits = require('util').inherits;
var EventEmitter = require('events').EventEmitter;
var SocksClient = require('./socks-client.js');

function SocksAgent(options, secure, rejectUnauthorized) {
    this.options = options;
    this.secure = secure || false;
    this.rejectUnauthorized = rejectUnauthorized;

    if (this.rejectUnauthorized === undefined) {
        this.rejectUnauthorized = true;
    }
}

inherits(SocksAgent, EventEmitter);

SocksAgent.prototype.createConnection = function(req, opts, fn) {
    var handler = fn, host, self = this;

    this.options.target = this.options.target || {};

    if (!this.options.target.host) {
        this.options.target.host = opts.host;
    }

    if (!this.options.target.port) {
        this.options.target.port = opts.port;
    }

    host = this.options.target.host;

    if (this.secure) {
        handler = function(err, socket, info) {
            var options, cleartext;

            if (err) {
                return fn(err);
            }

            // save encrypted socket
            self.encryptedSocket = socket;

            options = {
                socket: socket,
                servername: host,
                rejectUnauthorized: self.rejectUnauthorized
            };

            cleartext = tls.connect(options, function (err) {
                return fn(err, this);
            });
            cleartext.on('error', fn);

            socket.resume();
        }
    }

    SocksClient.createConnection(this.options, handler);
};

/**
 * @see https://www.npmjs.com/package/agent-base
 */
SocksAgent.prototype.addRequest = function(req, host, port, localAddress) {
    var opts;
    if ('object' === typeof host) {
        // >= v0.11.x API
        opts = host;
        if (opts.host && opts.path) {
            // if both a `host` and `path` are specified then it's most likely the
            // result of a `url.parse()` call... we need to remove the `path` portion so
            // that `net.connect()` doesn't attempt to open that as a unix socket file.
            delete opts.path;
        }
    } else {
        // <= v0.10.x API
        opts = { host: host, port: port };
        if (null !== localAddress) {
            opts.localAddress = localAddress;
        }
    }

    var sync = true;

    this.createConnection(req, opts, function (err, socket) {
        function emitErr () {
            req.emit('error', err);
        }
        if (err) {
            if (sync) {
                // need to defer the "error" event, when sync, because by now the `req`
                // instance hasn't event been passed back to the user yet...
                process.nextTick(emitErr);
            } else {
                emitErr();
            }
        } else {
            req.onSocket(socket);
            //have to resume this socket when node 12
            socket.resume();
        }
    });

    sync = false;
};

exports.Agent = SocksAgent;
