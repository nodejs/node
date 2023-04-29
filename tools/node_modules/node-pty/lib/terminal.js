"use strict";
/**
 * Copyright (c) 2012-2015, Christopher Jeffrey (MIT License)
 * Copyright (c) 2016, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 */
Object.defineProperty(exports, "__esModule", { value: true });
var events_1 = require("events");
var eventEmitter2_1 = require("./eventEmitter2");
exports.DEFAULT_COLS = 80;
exports.DEFAULT_ROWS = 24;
/**
 * Default messages to indicate PAUSE/RESUME for automatic flow control.
 * To avoid conflicts with rebound XON/XOFF control codes (such as on-my-zsh),
 * the sequences can be customized in `IPtyForkOptions`.
 */
var FLOW_CONTROL_PAUSE = '\x13'; // defaults to XOFF
var FLOW_CONTROL_RESUME = '\x11'; // defaults to XON
var Terminal = /** @class */ (function () {
    function Terminal(opt) {
        this._onData = new eventEmitter2_1.EventEmitter2();
        this._onExit = new eventEmitter2_1.EventEmitter2();
        // for 'close'
        this._internalee = new events_1.EventEmitter();
        if (!opt) {
            return;
        }
        // Do basic type checks here in case node-pty is being used within JavaScript. If the wrong
        // types go through to the C++ side it can lead to hard to diagnose exceptions.
        this._checkType('name', opt.name ? opt.name : undefined, 'string');
        this._checkType('cols', opt.cols ? opt.cols : undefined, 'number');
        this._checkType('rows', opt.rows ? opt.rows : undefined, 'number');
        this._checkType('cwd', opt.cwd ? opt.cwd : undefined, 'string');
        this._checkType('env', opt.env ? opt.env : undefined, 'object');
        this._checkType('uid', opt.uid ? opt.uid : undefined, 'number');
        this._checkType('gid', opt.gid ? opt.gid : undefined, 'number');
        this._checkType('encoding', opt.encoding ? opt.encoding : undefined, 'string');
        // setup flow control handling
        this.handleFlowControl = !!(opt.handleFlowControl);
        this._flowControlPause = opt.flowControlPause || FLOW_CONTROL_PAUSE;
        this._flowControlResume = opt.flowControlResume || FLOW_CONTROL_RESUME;
    }
    Object.defineProperty(Terminal.prototype, "onData", {
        get: function () { return this._onData.event; },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(Terminal.prototype, "onExit", {
        get: function () { return this._onExit.event; },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(Terminal.prototype, "pid", {
        get: function () { return this._pid; },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(Terminal.prototype, "cols", {
        get: function () { return this._cols; },
        enumerable: true,
        configurable: true
    });
    Object.defineProperty(Terminal.prototype, "rows", {
        get: function () { return this._rows; },
        enumerable: true,
        configurable: true
    });
    Terminal.prototype.write = function (data) {
        if (this.handleFlowControl) {
            // PAUSE/RESUME messages are not forwarded to the pty
            if (data === this._flowControlPause) {
                this.pause();
                return;
            }
            if (data === this._flowControlResume) {
                this.resume();
                return;
            }
        }
        // everything else goes to the real pty
        this._write(data);
    };
    Terminal.prototype._forwardEvents = function () {
        var _this = this;
        this.on('data', function (e) { return _this._onData.fire(e); });
        this.on('exit', function (exitCode, signal) { return _this._onExit.fire({ exitCode: exitCode, signal: signal }); });
    };
    Terminal.prototype._checkType = function (name, value, type, allowArray) {
        if (allowArray === void 0) { allowArray = false; }
        if (value === undefined) {
            return;
        }
        if (allowArray) {
            if (Array.isArray(value)) {
                value.forEach(function (v, i) {
                    if (typeof v !== type) {
                        throw new Error(name + "[" + i + "] must be a " + type + " (not a " + typeof v[i] + ")");
                    }
                });
                return;
            }
        }
        if (typeof value !== type) {
            throw new Error(name + " must be a " + type + " (not a " + typeof value + ")");
        }
    };
    /** See net.Socket.end */
    Terminal.prototype.end = function (data) {
        this._socket.end(data);
    };
    /** See stream.Readable.pipe */
    Terminal.prototype.pipe = function (dest, options) {
        return this._socket.pipe(dest, options);
    };
    /** See net.Socket.pause */
    Terminal.prototype.pause = function () {
        return this._socket.pause();
    };
    /** See net.Socket.resume */
    Terminal.prototype.resume = function () {
        return this._socket.resume();
    };
    /** See net.Socket.setEncoding */
    Terminal.prototype.setEncoding = function (encoding) {
        if (this._socket._decoder) {
            delete this._socket._decoder;
        }
        if (encoding) {
            this._socket.setEncoding(encoding);
        }
    };
    Terminal.prototype.addListener = function (eventName, listener) { this.on(eventName, listener); };
    Terminal.prototype.on = function (eventName, listener) {
        if (eventName === 'close') {
            this._internalee.on('close', listener);
            return;
        }
        this._socket.on(eventName, listener);
    };
    Terminal.prototype.emit = function (eventName) {
        var args = [];
        for (var _i = 1; _i < arguments.length; _i++) {
            args[_i - 1] = arguments[_i];
        }
        if (eventName === 'close') {
            return this._internalee.emit.apply(this._internalee, arguments);
        }
        return this._socket.emit.apply(this._socket, arguments);
    };
    Terminal.prototype.listeners = function (eventName) {
        return this._socket.listeners(eventName);
    };
    Terminal.prototype.removeListener = function (eventName, listener) {
        this._socket.removeListener(eventName, listener);
    };
    Terminal.prototype.removeAllListeners = function (eventName) {
        this._socket.removeAllListeners(eventName);
    };
    Terminal.prototype.once = function (eventName, listener) {
        this._socket.once(eventName, listener);
    };
    Terminal.prototype._close = function () {
        this._socket.writable = false;
        this._socket.readable = false;
        this.write = function () { };
        this.end = function () { };
        this._writable = false;
        this._readable = false;
    };
    Terminal.prototype._parseEnv = function (env) {
        var keys = Object.keys(env || {});
        var pairs = [];
        for (var i = 0; i < keys.length; i++) {
            pairs.push(keys[i] + '=' + env[keys[i]]);
        }
        return pairs;
    };
    return Terminal;
}());
exports.Terminal = Terminal;
//# sourceMappingURL=terminal.js.map