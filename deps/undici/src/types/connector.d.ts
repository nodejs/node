import { URL } from 'url'
import { TLSSocket, TlsOptions } from 'tls'
import { Socket } from 'net'

export = buildConnector
declare function buildConnector (options?: buildConnector.BuildOptions): typeof buildConnector.connector

declare namespace buildConnector {
  export interface BuildOptions extends TlsOptions {
    maxCachedSessions?: number | null;
    socketPath?: string | null;
    timeout?: number | null;
    servername?: string | null;
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
