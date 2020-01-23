/// <reference types="node" />
import { EventEmitter } from 'events';
import { SocksClientOptions, SocksClientChainOptions, SocksRemoteHost, SocksProxy, SocksClientBoundEvent, SocksClientEstablishedEvent, SocksUDPFrameDetails } from '../common/constants';
import { SocksClientError } from '../common/util';
import { Duplex } from 'stream';
declare interface SocksClient {
    on(event: 'error', listener: (err: SocksClientError) => void): this;
    on(event: 'bound', listener: (info: SocksClientBoundEvent) => void): this;
    on(event: 'established', listener: (info: SocksClientEstablishedEvent) => void): this;
    once(event: string, listener: (...args: any[]) => void): this;
    once(event: 'error', listener: (err: SocksClientError) => void): this;
    once(event: 'bound', listener: (info: SocksClientBoundEvent) => void): this;
    once(event: 'established', listener: (info: SocksClientEstablishedEvent) => void): this;
    emit(event: string | symbol, ...args: any[]): boolean;
    emit(event: 'error', err: SocksClientError): boolean;
    emit(event: 'bound', info: SocksClientBoundEvent): boolean;
    emit(event: 'established', info: SocksClientEstablishedEvent): boolean;
}
declare class SocksClient extends EventEmitter implements SocksClient {
    private _options;
    private _socket;
    private _state;
    private _receiveBuffer;
    private _nextRequiredPacketBufferSize;
    private _onDataReceived;
    private _onClose;
    private _onError;
    private _onConnect;
    constructor(options: SocksClientOptions);
    /**
     * Creates a new SOCKS connection.
     *
     * Note: Supports callbacks and promises. Only supports the connect command.
     * @param options { SocksClientOptions } Options.
     * @param callback { Function } An optional callback function.
     * @returns { Promise }
     */
    static createConnection(options: SocksClientOptions, callback?: Function): Promise<SocksClientEstablishedEvent>;
    /**
     * Creates a new SOCKS connection chain to a destination host through 2 or more SOCKS proxies.
     *
     * Note: Supports callbacks and promises. Only supports the connect method.
     * Note: Implemented via createConnection() factory function.
     * @param options { SocksClientChainOptions } Options
     * @param callback { Function } An optional callback function.
     * @returns { Promise }
     */
    static createConnectionChain(options: SocksClientChainOptions, callback?: Function): Promise<SocksClientEstablishedEvent>;
    /**
     * Creates a SOCKS UDP Frame.
     * @param options
     */
    static createUDPFrame(options: SocksUDPFrameDetails): Buffer;
    /**
     * Parses a SOCKS UDP frame.
     * @param data
     */
    static parseUDPFrame(data: Buffer): SocksUDPFrameDetails;
    /**
     * Gets the SocksClient internal state.
     */
    private get state();
    /**
     * Internal state setter. If the SocksClient is in an error state, it cannot be changed to a non error state.
     */
    private set state(value);
    /**
     * Starts the connection establishment to the proxy and destination.
     * @param existing_socket Connected socket to use instead of creating a new one (internal use).
     */
    connect(existing_socket?: Duplex): void;
    private getSocketOptions;
    /**
     * Handles internal Socks timeout callback.
     * Note: If the Socks client is not BoundWaitingForConnection or Established, the connection will be closed.
     */
    private onEstablishedTimeout;
    /**
     * Handles Socket connect event.
     */
    private onConnect;
    /**
     * Handles Socket data event.
     * @param data
     */
    private onDataReceived;
    /**
     * Handles processing of the data we have received.
     */
    private processData;
    /**
     * Handles Socket close event.
     * @param had_error
     */
    private onClose;
    /**
     * Handles Socket error event.
     * @param err
     */
    private onError;
    /**
     * Removes internal event listeners on the underlying Socket.
     */
    private removeInternalSocketHandlers;
    /**
     * Closes and destroys the underlying Socket. Emits an error event.
     * @param err { String } An error string to include in error event.
     */
    private _closeSocket;
    /**
     * Sends initial Socks v4 handshake request.
     */
    private sendSocks4InitialHandshake;
    /**
     * Handles Socks v4 handshake response.
     * @param data
     */
    private handleSocks4FinalHandshakeResponse;
    /**
     * Handles Socks v4 incoming connection request (BIND)
     * @param data
     */
    private handleSocks4IncomingConnectionResponse;
    /**
     * Sends initial Socks v5 handshake request.
     */
    private sendSocks5InitialHandshake;
    /**
     * Handles initial Socks v5 handshake response.
     * @param data
     */
    private handleInitialSocks5HandshakeResponse;
    /**
     * Sends Socks v5 user & password auth handshake.
     *
     * Note: No auth and user/pass are currently supported.
     */
    private sendSocks5UserPassAuthentication;
    /**
     * Handles Socks v5 auth handshake response.
     * @param data
     */
    private handleInitialSocks5AuthenticationHandshakeResponse;
    /**
     * Sends Socks v5 final handshake request.
     */
    private sendSocks5CommandRequest;
    /**
     * Handles Socks v5 final handshake response.
     * @param data
     */
    private handleSocks5FinalHandshakeResponse;
    /**
     * Handles Socks v5 incoming connection request (BIND).
     */
    private handleSocks5IncomingConnectionResponse;
    get socksClientOptions(): SocksClientOptions;
}
export { SocksClient, SocksClientOptions, SocksClientChainOptions, SocksRemoteHost, SocksProxy, SocksUDPFrameDetails };
