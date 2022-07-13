import {TLSSocket, ConnectionOptions} from 'tls'
import {IpcNetConnectOpts, Socket, TcpNetConnectOpts} from 'net'

export = buildConnector
declare function buildConnector (options?: buildConnector.BuildOptions): typeof buildConnector.connector

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

  export type Callback = (err: Error | null, socket: Socket | TLSSocket | null) => void

  export function connector (options: buildConnector.Options, callback: buildConnector.Callback): Socket | TLSSocket;
}
