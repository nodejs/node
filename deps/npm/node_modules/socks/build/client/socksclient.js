"use strict";
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : new P(function (resolve) { resolve(result.value); }).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
const events_1 = require("events");
const net = require("net");
const ip = require("ip");
const smart_buffer_1 = require("smart-buffer");
const constants_1 = require("../common/constants");
const helpers_1 = require("../common/helpers");
const receivebuffer_1 = require("../common/receivebuffer");
const util_1 = require("../common/util");
class SocksClient extends events_1.EventEmitter {
    constructor(options) {
        super();
        this._options = Object.assign({}, options);
        // Validate SocksClientOptions
        helpers_1.validateSocksClientOptions(options);
        // Default state
        this.state = constants_1.SocksClientState.Created;
    }
    /**
     * Creates a new SOCKS connection.
     *
     * Note: Supports callbacks and promises. Only supports the connect command.
     * @param options { SocksClientOptions } Options.
     * @param callback { Function } An optional callback function.
     * @returns { Promise }
     */
    static createConnection(options, callback) {
        // Validate SocksClientOptions
        helpers_1.validateSocksClientOptions(options, ['connect']);
        return new Promise((resolve, reject) => {
            const client = new SocksClient(options);
            client.connect(options.existing_socket);
            client.once('established', (info) => {
                client.removeAllListeners();
                if (typeof callback === 'function') {
                    callback(null, info);
                    resolve(); // Resolves pending promise (prevents memory leaks).
                }
                else {
                    resolve(info);
                }
            });
            // Error occurred, failed to establish connection.
            client.once('error', (err) => {
                client.removeAllListeners();
                if (typeof callback === 'function') {
                    callback(err);
                    resolve(); // Resolves pending promise (prevents memory leaks).
                }
                else {
                    reject(err);
                }
            });
        });
    }
    /**
     * Creates a new SOCKS connection chain to a destination host through 2 or more SOCKS proxies.
     *
     * Note: Supports callbacks and promises. Only supports the connect method.
     * Note: Implemented via createConnection() factory function.
     * @param options { SocksClientChainOptions } Options
     * @param callback { Function } An optional callback function.
     * @returns { Promise }
     */
    static createConnectionChain(options, callback) {
        // Validate SocksClientChainOptions
        helpers_1.validateSocksClientChainOptions(options);
        // Shuffle proxies
        if (options.randomizeChain) {
            util_1.shuffleArray(options.proxies);
        }
        return new Promise((resolve, reject) => __awaiter(this, void 0, void 0, function* () {
            let sock;
            try {
                for (let i = 0; i < options.proxies.length; i++) {
                    const nextProxy = options.proxies[i];
                    // If we've reached the last proxy in the chain, the destination is the actual destination, otherwise it's the next proxy.
                    const nextDestination = i === options.proxies.length - 1
                        ? options.destination
                        : {
                            host: options.proxies[i + 1].ipaddress,
                            port: options.proxies[i + 1].port
                        };
                    // Creates the next connection in the chain.
                    const result = yield SocksClient.createConnection({
                        command: 'connect',
                        proxy: nextProxy,
                        destination: nextDestination
                        // Initial connection ignores this as sock is undefined. Subsequent connections re-use the first proxy socket to form a chain.
                    });
                    // If sock is undefined, assign it here.
                    if (!sock) {
                        sock = result.socket;
                    }
                }
                if (typeof callback === 'function') {
                    callback(null, { socket: sock });
                    resolve(); // Resolves pending promise (prevents memory leaks).
                }
                else {
                    resolve({ socket: sock });
                }
            }
            catch (err) {
                if (typeof callback === 'function') {
                    callback(err);
                    resolve(); // Resolves pending promise (prevents memory leaks).
                }
                else {
                    reject(err);
                }
            }
        }));
    }
    /**
     * Creates a SOCKS UDP Frame.
     * @param options
     */
    static createUDPFrame(options) {
        const buff = new smart_buffer_1.SmartBuffer();
        buff.writeUInt16BE(0);
        buff.writeUInt8(options.frameNumber || 0);
        // IPv4/IPv6/Hostname
        if (net.isIPv4(options.remoteHost.host)) {
            buff.writeUInt8(constants_1.Socks5HostType.IPv4);
            buff.writeUInt32BE(ip.toLong(options.remoteHost.host));
        }
        else if (net.isIPv6(options.remoteHost.host)) {
            buff.writeUInt8(constants_1.Socks5HostType.IPv6);
            buff.writeBuffer(ip.toBuffer(options.remoteHost.host));
        }
        else {
            buff.writeUInt8(constants_1.Socks5HostType.Hostname);
            buff.writeUInt8(Buffer.byteLength(options.remoteHost.host));
            buff.writeString(options.remoteHost.host);
        }
        // Port
        buff.writeUInt16BE(options.remoteHost.port);
        // Data
        buff.writeBuffer(options.data);
        return buff.toBuffer();
    }
    /**
     * Parses a SOCKS UDP frame.
     * @param data
     */
    static parseUDPFrame(data) {
        const buff = smart_buffer_1.SmartBuffer.fromBuffer(data);
        buff.readOffset = 2;
        const frameNumber = buff.readUInt8();
        const hostType = buff.readUInt8();
        let remoteHost;
        if (hostType === constants_1.Socks5HostType.IPv4) {
            remoteHost = ip.fromLong(buff.readUInt32BE());
        }
        else if (hostType === constants_1.Socks5HostType.IPv6) {
            remoteHost = ip.toString(buff.readBuffer(16));
        }
        else {
            remoteHost = buff.readString(buff.readUInt8());
        }
        const remotePort = buff.readUInt16BE();
        return {
            frameNumber,
            remoteHost: {
                host: remoteHost,
                port: remotePort
            },
            data: buff.readBuffer()
        };
    }
    /**
     * Gets the SocksClient internal state.
     */
    get state() {
        return this._state;
    }
    /**
     * Internal state setter. If the SocksClient is in an error state, it cannot be changed to a non error state.
     */
    set state(newState) {
        if (this._state !== constants_1.SocksClientState.Error) {
            this._state = newState;
        }
    }
    /**
     * Starts the connection establishment to the proxy and destination.
     * @param existing_socket Connected socket to use instead of creating a new one (internal use).
     */
    connect(existing_socket) {
        this._onDataReceived = (data) => this.onDataReceived(data);
        this._onClose = () => this.onClose();
        this._onError = (err) => this.onError(err);
        this._onConnect = () => this.onConnect();
        // Start timeout timer (defaults to 30 seconds)
        const timer = setTimeout(() => this.onEstablishedTimeout(), this._options.timeout || constants_1.DEFAULT_TIMEOUT);
        // check whether unref is available as it differs from browser to NodeJS (#33)
        if (timer.unref && typeof timer.unref === 'function') {
            timer.unref();
        }
        // If an existing socket is provided, use it to negotiate SOCKS handshake. Otherwise create a new Socket.
        if (existing_socket) {
            this._socket = existing_socket;
        }
        else {
            this._socket = new net.Socket();
        }
        // Attach Socket error handlers.
        this._socket.once('close', this._onClose);
        this._socket.once('error', this._onError);
        this._socket.once('connect', this._onConnect);
        this._socket.on('data', this._onDataReceived);
        this.state = constants_1.SocksClientState.Connecting;
        this._receiveBuffer = new receivebuffer_1.ReceiveBuffer();
        if (existing_socket) {
            this._socket.emit('connect');
        }
        else {
            this._socket.connect(this._options.proxy.port, this._options.proxy.host || this._options.proxy.ipaddress);
            if (this._options.set_tcp_nodelay !== undefined &&
                this._options.set_tcp_nodelay !== null) {
                this._socket.setNoDelay(!!this._options.set_tcp_nodelay);
            }
        }
        // Listen for established event so we can re-emit any excess data received during handshakes.
        this.prependOnceListener('established', info => {
            setImmediate(() => {
                if (this._receiveBuffer.length > 0) {
                    const excessData = this._receiveBuffer.get(this._receiveBuffer.length);
                    info.socket.emit('data', excessData);
                }
                info.socket.resume();
            });
        });
    }
    /**
     * Handles internal Socks timeout callback.
     * Note: If the Socks client is not BoundWaitingForConnection or Established, the connection will be closed.
     */
    onEstablishedTimeout() {
        if (this.state !== constants_1.SocksClientState.Established &&
            this.state !== constants_1.SocksClientState.BoundWaitingForConnection) {
            this._closeSocket(constants_1.ERRORS.ProxyConnectionTimedOut);
        }
    }
    /**
     * Handles Socket connect event.
     */
    onConnect() {
        this.state = constants_1.SocksClientState.Connected;
        // Send initial handshake.
        if (this._options.proxy.type === 4) {
            this.sendSocks4InitialHandshake();
        }
        else {
            this.sendSocks5InitialHandshake();
        }
        this.state = constants_1.SocksClientState.SentInitialHandshake;
    }
    /**
     * Handles Socket data event.
     * @param data
     */
    onDataReceived(data) {
        /*
          All received data is appended to a ReceiveBuffer.
          This makes sure that all the data we need is received before we attempt to process it.
        */
        this._receiveBuffer.append(data);
        // Process data that we have.
        this.processData();
    }
    /**
     * Handles processing of the data we have received.
     */
    processData() {
        // If we have enough data to process the next step in the SOCKS handshake, proceed.
        if (this._receiveBuffer.length >= this._nextRequiredPacketBufferSize) {
            // Sent initial handshake, waiting for response.
            if (this.state === constants_1.SocksClientState.SentInitialHandshake) {
                if (this._options.proxy.type === 4) {
                    // Socks v4 only has one handshake response.
                    this.handleSocks4FinalHandshakeResponse();
                }
                else {
                    // Socks v5 has two handshakes, handle initial one here.
                    this.handleInitialSocks5HandshakeResponse();
                }
                // Sent auth request for Socks v5, waiting for response.
            }
            else if (this.state === constants_1.SocksClientState.SentAuthentication) {
                this.handleInitialSocks5AuthenticationHandshakeResponse();
                // Sent final Socks v5 handshake, waiting for final response.
            }
            else if (this.state === constants_1.SocksClientState.SentFinalHandshake) {
                this.handleSocks5FinalHandshakeResponse();
                // Socks BIND established. Waiting for remote connection via proxy.
            }
            else if (this.state === constants_1.SocksClientState.BoundWaitingForConnection) {
                if (this._options.proxy.type === 4) {
                    this.handleSocks4IncomingConnectionResponse();
                }
                else {
                    this.handleSocks5IncomingConnectionResponse();
                }
            }
            else if (this.state === constants_1.SocksClientState.Established) {
                // do nothing (prevents closing of the socket)
            }
            else {
                this._closeSocket(constants_1.ERRORS.InternalError);
            }
        }
    }
    /**
     * Handles Socket close event.
     * @param had_error
     */
    onClose() {
        this._closeSocket(constants_1.ERRORS.SocketClosed);
    }
    /**
     * Handles Socket error event.
     * @param err
     */
    onError(err) {
        this._closeSocket(err.message);
    }
    /**
     * Removes internal event listeners on the underlying Socket.
     */
    removeInternalSocketHandlers() {
        // Pauses data flow of the socket (this is internally resumed after 'established' is emitted)
        this._socket.pause();
        this._socket.removeListener('data', this._onDataReceived);
        this._socket.removeListener('close', this._onClose);
        this._socket.removeListener('error', this._onError);
        this._socket.removeListener('connect', this.onConnect);
    }
    /**
     * Closes and destroys the underlying Socket. Emits an error event.
     * @param err { String } An error string to include in error event.
     */
    _closeSocket(err) {
        // Make sure only one 'error' event is fired for the lifetime of this SocksClient instance.
        if (this.state !== constants_1.SocksClientState.Error) {
            // Set internal state to Error.
            this.state = constants_1.SocksClientState.Error;
            // Destroy Socket
            this._socket.destroy();
            // Remove internal listeners
            this.removeInternalSocketHandlers();
            // Fire 'error' event.
            this.emit('error', new util_1.SocksClientError(err, this._options));
        }
    }
    /**
     * Sends initial Socks v4 handshake request.
     */
    sendSocks4InitialHandshake() {
        const userId = this._options.proxy.userId || '';
        const buff = new smart_buffer_1.SmartBuffer();
        buff.writeUInt8(0x04);
        buff.writeUInt8(constants_1.SocksCommand[this._options.command]);
        buff.writeUInt16BE(this._options.destination.port);
        // Socks 4 (IPv4)
        if (net.isIPv4(this._options.destination.host)) {
            buff.writeBuffer(ip.toBuffer(this._options.destination.host));
            buff.writeStringNT(userId);
            // Socks 4a (hostname)
        }
        else {
            buff.writeUInt8(0x00);
            buff.writeUInt8(0x00);
            buff.writeUInt8(0x00);
            buff.writeUInt8(0x01);
            buff.writeStringNT(userId);
            buff.writeStringNT(this._options.destination.host);
        }
        this._nextRequiredPacketBufferSize =
            constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks4Response;
        this._socket.write(buff.toBuffer());
    }
    /**
     * Handles Socks v4 handshake response.
     * @param data
     */
    handleSocks4FinalHandshakeResponse() {
        const data = this._receiveBuffer.get(8);
        if (data[1] !== constants_1.Socks4Response.Granted) {
            this._closeSocket(`${constants_1.ERRORS.Socks4ProxyRejectedConnection} - (${constants_1.Socks4Response[data[1]]})`);
        }
        else {
            // Bind response
            if (constants_1.SocksCommand[this._options.command] === constants_1.SocksCommand.bind) {
                const buff = smart_buffer_1.SmartBuffer.fromBuffer(data);
                buff.readOffset = 2;
                const remoteHost = {
                    port: buff.readUInt16BE(),
                    host: ip.fromLong(buff.readUInt32BE())
                };
                // If host is 0.0.0.0, set to proxy host.
                if (remoteHost.host === '0.0.0.0') {
                    remoteHost.host = this._options.proxy.ipaddress;
                }
                this.state = constants_1.SocksClientState.BoundWaitingForConnection;
                this.emit('bound', { socket: this._socket, remoteHost });
                // Connect response
            }
            else {
                this.state = constants_1.SocksClientState.Established;
                this.removeInternalSocketHandlers();
                this.emit('established', { socket: this._socket });
            }
        }
    }
    /**
     * Handles Socks v4 incoming connection request (BIND)
     * @param data
     */
    handleSocks4IncomingConnectionResponse() {
        const data = this._receiveBuffer.get(8);
        if (data[1] !== constants_1.Socks4Response.Granted) {
            this._closeSocket(`${constants_1.ERRORS.Socks4ProxyRejectedIncomingBoundConnection} - (${constants_1.Socks4Response[data[1]]})`);
        }
        else {
            const buff = smart_buffer_1.SmartBuffer.fromBuffer(data);
            buff.readOffset = 2;
            const remoteHost = {
                port: buff.readUInt16BE(),
                host: ip.fromLong(buff.readUInt32BE())
            };
            this.state = constants_1.SocksClientState.Established;
            this.removeInternalSocketHandlers();
            this.emit('established', { socket: this._socket, remoteHost });
        }
    }
    /**
     * Sends initial Socks v5 handshake request.
     */
    sendSocks5InitialHandshake() {
        const buff = new smart_buffer_1.SmartBuffer();
        buff.writeUInt8(0x05);
        // We should only tell the proxy we support user/pass auth if auth info is actually provided.
        // Note: As of Tor v0.3.5.7+, if user/pass auth is an option from the client, by default it will always take priority.
        if (this._options.proxy.userId || this._options.proxy.password) {
            buff.writeUInt8(2);
            buff.writeUInt8(constants_1.Socks5Auth.NoAuth);
            buff.writeUInt8(constants_1.Socks5Auth.UserPass);
        }
        else {
            buff.writeUInt8(1);
            buff.writeUInt8(constants_1.Socks5Auth.NoAuth);
        }
        this._nextRequiredPacketBufferSize =
            constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5InitialHandshakeResponse;
        this._socket.write(buff.toBuffer());
        this.state = constants_1.SocksClientState.SentInitialHandshake;
    }
    /**
     * Handles initial Socks v5 handshake response.
     * @param data
     */
    handleInitialSocks5HandshakeResponse() {
        const data = this._receiveBuffer.get(2);
        if (data[0] !== 0x05) {
            this._closeSocket(constants_1.ERRORS.InvalidSocks5IntiailHandshakeSocksVersion);
        }
        else if (data[1] === 0xff) {
            this._closeSocket(constants_1.ERRORS.InvalidSocks5InitialHandshakeNoAcceptedAuthType);
        }
        else {
            // If selected Socks v5 auth method is no auth, send final handshake request.
            if (data[1] === constants_1.Socks5Auth.NoAuth) {
                this.sendSocks5CommandRequest();
                // If selected Socks v5 auth method is user/password, send auth handshake.
            }
            else if (data[1] === constants_1.Socks5Auth.UserPass) {
                this.sendSocks5UserPassAuthentication();
            }
            else {
                this._closeSocket(constants_1.ERRORS.InvalidSocks5InitialHandshakeUnknownAuthType);
            }
        }
    }
    /**
     * Sends Socks v5 user & password auth handshake.
     *
     * Note: No auth and user/pass are currently supported.
     */
    sendSocks5UserPassAuthentication() {
        const userId = this._options.proxy.userId || '';
        const password = this._options.proxy.password || '';
        const buff = new smart_buffer_1.SmartBuffer();
        buff.writeUInt8(0x01);
        buff.writeUInt8(Buffer.byteLength(userId));
        buff.writeString(userId);
        buff.writeUInt8(Buffer.byteLength(password));
        buff.writeString(password);
        this._nextRequiredPacketBufferSize =
            constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5UserPassAuthenticationResponse;
        this._socket.write(buff.toBuffer());
        this.state = constants_1.SocksClientState.SentAuthentication;
    }
    /**
     * Handles Socks v5 auth handshake response.
     * @param data
     */
    handleInitialSocks5AuthenticationHandshakeResponse() {
        this.state = constants_1.SocksClientState.ReceivedAuthenticationResponse;
        const data = this._receiveBuffer.get(2);
        if (data[1] !== 0x00) {
            this._closeSocket(constants_1.ERRORS.Socks5AuthenticationFailed);
        }
        else {
            this.sendSocks5CommandRequest();
        }
    }
    /**
     * Sends Socks v5 final handshake request.
     */
    sendSocks5CommandRequest() {
        const buff = new smart_buffer_1.SmartBuffer();
        buff.writeUInt8(0x05);
        buff.writeUInt8(constants_1.SocksCommand[this._options.command]);
        buff.writeUInt8(0x00);
        // ipv4, ipv6, domain?
        if (net.isIPv4(this._options.destination.host)) {
            buff.writeUInt8(constants_1.Socks5HostType.IPv4);
            buff.writeBuffer(ip.toBuffer(this._options.destination.host));
        }
        else if (net.isIPv6(this._options.destination.host)) {
            buff.writeUInt8(constants_1.Socks5HostType.IPv6);
            buff.writeBuffer(ip.toBuffer(this._options.destination.host));
        }
        else {
            buff.writeUInt8(constants_1.Socks5HostType.Hostname);
            buff.writeUInt8(this._options.destination.host.length);
            buff.writeString(this._options.destination.host);
        }
        buff.writeUInt16BE(this._options.destination.port);
        this._nextRequiredPacketBufferSize =
            constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5ResponseHeader;
        this._socket.write(buff.toBuffer());
        this.state = constants_1.SocksClientState.SentFinalHandshake;
    }
    /**
     * Handles Socks v5 final handshake response.
     * @param data
     */
    handleSocks5FinalHandshakeResponse() {
        // Peek at available data (we need at least 5 bytes to get the hostname length)
        const header = this._receiveBuffer.peek(5);
        if (header[0] !== 0x05 || header[1] !== constants_1.Socks5Response.Granted) {
            this._closeSocket(`${constants_1.ERRORS.InvalidSocks5FinalHandshakeRejected} - ${constants_1.Socks5Response[header[1]]}`);
        }
        else {
            // Read address type
            const addressType = header[3];
            let remoteHost;
            let buff;
            // IPv4
            if (addressType === constants_1.Socks5HostType.IPv4) {
                // Check if data is available.
                const dataNeeded = constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5ResponseIPv4;
                if (this._receiveBuffer.length < dataNeeded) {
                    this._nextRequiredPacketBufferSize = dataNeeded;
                    return;
                }
                buff = smart_buffer_1.SmartBuffer.fromBuffer(this._receiveBuffer.get(dataNeeded).slice(4));
                remoteHost = {
                    host: ip.fromLong(buff.readUInt32BE()),
                    port: buff.readUInt16BE()
                };
                // If given host is 0.0.0.0, assume remote proxy ip instead.
                if (remoteHost.host === '0.0.0.0') {
                    remoteHost.host = this._options.proxy.ipaddress;
                }
                // Hostname
            }
            else if (addressType === constants_1.Socks5HostType.Hostname) {
                const hostLength = header[4];
                const dataNeeded = constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5ResponseHostname(hostLength); // header + host length + host + port
                // Check if data is available.
                if (this._receiveBuffer.length < dataNeeded) {
                    this._nextRequiredPacketBufferSize = dataNeeded;
                    return;
                }
                buff = smart_buffer_1.SmartBuffer.fromBuffer(this._receiveBuffer.get(dataNeeded).slice(5) // Slice at 5 to skip host length
                );
                remoteHost = {
                    host: buff.readString(hostLength),
                    port: buff.readUInt16BE()
                };
                // IPv6
            }
            else if (addressType === constants_1.Socks5HostType.IPv6) {
                // Check if data is available.
                const dataNeeded = constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5ResponseIPv6;
                if (this._receiveBuffer.length < dataNeeded) {
                    this._nextRequiredPacketBufferSize = dataNeeded;
                    return;
                }
                buff = smart_buffer_1.SmartBuffer.fromBuffer(this._receiveBuffer.get(dataNeeded).slice(4));
                remoteHost = {
                    host: ip.toString(buff.readBuffer(16)),
                    port: buff.readUInt16BE()
                };
            }
            // We have everything we need
            this.state = constants_1.SocksClientState.ReceivedFinalResponse;
            // If using CONNECT, the client is now in the established state.
            if (constants_1.SocksCommand[this._options.command] === constants_1.SocksCommand.connect) {
                this.state = constants_1.SocksClientState.Established;
                this.removeInternalSocketHandlers();
                this.emit('established', { socket: this._socket });
            }
            else if (constants_1.SocksCommand[this._options.command] === constants_1.SocksCommand.bind) {
                /* If using BIND, the Socks client is now in BoundWaitingForConnection state.
                   This means that the remote proxy server is waiting for a remote connection to the bound port. */
                this.state = constants_1.SocksClientState.BoundWaitingForConnection;
                this._nextRequiredPacketBufferSize =
                    constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5ResponseHeader;
                this.emit('bound', { socket: this._socket, remoteHost });
                /*
                  If using Associate, the Socks client is now Established. And the proxy server is now accepting UDP packets at the
                  given bound port. This initial Socks TCP connection must remain open for the UDP relay to continue to work.
                */
            }
            else if (constants_1.SocksCommand[this._options.command] === constants_1.SocksCommand.associate) {
                this.state = constants_1.SocksClientState.Established;
                this.removeInternalSocketHandlers();
                this.emit('established', { socket: this._socket, remoteHost });
            }
        }
    }
    /**
     * Handles Socks v5 incoming connection request (BIND).
     */
    handleSocks5IncomingConnectionResponse() {
        // Peek at available data (we need at least 5 bytes to get the hostname length)
        const header = this._receiveBuffer.peek(5);
        if (header[0] !== 0x05 || header[1] !== constants_1.Socks5Response.Granted) {
            this._closeSocket(`${constants_1.ERRORS.Socks5ProxyRejectedIncomingBoundConnection} - ${constants_1.Socks5Response[header[1]]}`);
        }
        else {
            // Read address type
            const addressType = header[3];
            let remoteHost;
            let buff;
            // IPv4
            if (addressType === constants_1.Socks5HostType.IPv4) {
                // Check if data is available.
                const dataNeeded = constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5ResponseIPv4;
                if (this._receiveBuffer.length < dataNeeded) {
                    this._nextRequiredPacketBufferSize = dataNeeded;
                    return;
                }
                buff = smart_buffer_1.SmartBuffer.fromBuffer(this._receiveBuffer.get(dataNeeded).slice(4));
                remoteHost = {
                    host: ip.fromLong(buff.readUInt32BE()),
                    port: buff.readUInt16BE()
                };
                // If given host is 0.0.0.0, assume remote proxy ip instead.
                if (remoteHost.host === '0.0.0.0') {
                    remoteHost.host = this._options.proxy.ipaddress;
                }
                // Hostname
            }
            else if (addressType === constants_1.Socks5HostType.Hostname) {
                const hostLength = header[4];
                const dataNeeded = constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5ResponseHostname(hostLength); // header + host length + port
                // Check if data is available.
                if (this._receiveBuffer.length < dataNeeded) {
                    this._nextRequiredPacketBufferSize = dataNeeded;
                    return;
                }
                buff = smart_buffer_1.SmartBuffer.fromBuffer(this._receiveBuffer.get(dataNeeded).slice(5) // Slice at 5 to skip host length
                );
                remoteHost = {
                    host: buff.readString(hostLength),
                    port: buff.readUInt16BE()
                };
                // IPv6
            }
            else if (addressType === constants_1.Socks5HostType.IPv6) {
                // Check if data is available.
                const dataNeeded = constants_1.SOCKS_INCOMING_PACKET_SIZES.Socks5ResponseIPv6;
                if (this._receiveBuffer.length < dataNeeded) {
                    this._nextRequiredPacketBufferSize = dataNeeded;
                    return;
                }
                buff = smart_buffer_1.SmartBuffer.fromBuffer(this._receiveBuffer.get(dataNeeded).slice(4));
                remoteHost = {
                    host: ip.toString(buff.readBuffer(16)),
                    port: buff.readUInt16BE()
                };
            }
            this.state = constants_1.SocksClientState.Established;
            this.removeInternalSocketHandlers();
            this.emit('established', { socket: this._socket, remoteHost });
        }
    }
    get socksClientOptions() {
        return Object.assign({}, this._options);
    }
}
exports.SocksClient = SocksClient;
//# sourceMappingURL=socksclient.js.map