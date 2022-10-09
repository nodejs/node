import {TLSSocket, ConnectionOptions} from 'tls'
import {IpcNetConnectOpts, Socket, TcpNetConnectOpts} from 'net'

export = buildConnector
declare function buildConnector (options?: buildConnector.BuildOptions): buildConnector.connector

declare namespace buildConnector {
  export type BuildOptions = (ConnectionOptions | TcpNetConnectOpts | IpcNetConnectOpts) & {
    maxCachedSessions?: number | null;
    socketPath?: string | null;
    timeout?: number | null;
    port?: number;
  }

  export interface Options {
    hostname: string
    host?: string
    protocol: string
    port: number
    servername?: string
  }

  export type Callback = (...args: CallbackArgs) => void
  type CallbackArgs = [null, Socket | TLSSocket] | [Error, null]

  export type connector = connectorAsync | connectorSync

  interface connectorSync {
    (options: buildConnector.Options): Socket | TLSSocket
  }

  interface connectorAsync {
    (options: buildConnector.Options, callback: buildConnector.Callback): void
  }
}
