import { URL } from 'url'
import { Duplex, Readable, Writable } from 'stream'
import { EventEmitter } from 'events'
import { IncomingHttpHeaders } from 'http'
import { Blob } from 'buffer'
import BodyReadable = require('./readable')
import { FormData } from './formdata'

type AbortSignal = unknown;

export = Dispatcher;

/** Dispatcher is the core API used to dispatch requests. */
declare class Dispatcher extends EventEmitter {
  /** Dispatches a request. This API is expected to evolve through semver-major versions and is less stable than the preceding higher level APIs. It is primarily intended for library developers who implement higher level APIs on top of this. */
  dispatch(options: Dispatcher.DispatchOptions, handler: Dispatcher.DispatchHandlers): boolean;
  /** Starts two-way communications with the requested resource. */
  connect(options: Dispatcher.ConnectOptions): Promise<Dispatcher.ConnectData>;
  connect(options: Dispatcher.ConnectOptions, callback: (err: Error | null, data: Dispatcher.ConnectData) => void): void;
  /** Performs an HTTP request. */
  request(options: Dispatcher.RequestOptions): Promise<Dispatcher.ResponseData>;
  request(options: Dispatcher.RequestOptions, callback: (err: Error | null, data: Dispatcher.ResponseData) => void): void;
  /** For easy use with `stream.pipeline`. */
  pipeline(options: Dispatcher.PipelineOptions, handler: Dispatcher.PipelineHandler): Duplex;
  /** A faster version of `Dispatcher.request`. */
  stream(options: Dispatcher.RequestOptions, factory: Dispatcher.StreamFactory): Promise<Dispatcher.StreamData>;
  stream(options: Dispatcher.RequestOptions, factory: Dispatcher.StreamFactory, callback: (err: Error | null, data: Dispatcher.StreamData) => void): void;
  /** Upgrade to a different protocol. */
  upgrade(options: Dispatcher.UpgradeOptions): Promise<Dispatcher.UpgradeData>;
  upgrade(options: Dispatcher.UpgradeOptions, callback: (err: Error | null, data: Dispatcher.UpgradeData) => void): void;
  /** Closes the client and gracefully waits for enqueued requests to complete before invoking the callback (or returning a promise if no callback is provided). */
  close(): Promise<void>;
  close(callback: () => void): void;
  /** Destroy the client abruptly with the given err. All the pending and running requests will be asynchronously aborted and error. Waits until socket is closed before invoking the callback (or returning a promise if no callback is provided). Since this operation is asynchronously dispatched there might still be some progress on dispatched requests. */
  destroy(): Promise<void>;
  destroy(err: Error | null): Promise<void>;
  destroy(callback: () => void): void;
  destroy(err: Error | null, callback: () => void): void;
}

declare namespace Dispatcher {
  export interface DispatchOptions {
    origin?: string | URL;
    path: string;
    method: HttpMethod;
    /** Default: `null` */
    body?: string | Buffer | Uint8Array | Readable | null | FormData;
    /** Default: `null` */
    headers?: IncomingHttpHeaders | string[] | null;
    /** Query string params to be embedded in the request URL. Default: `null` */
    query?: Record<string, any>;
    /** Whether the requests can be safely retried or not. If `false` the request won't be sent until all preceding requests in the pipeline have completed. Default: `true` if `method` is `HEAD` or `GET`. */
    idempotent?: boolean;
    /** Upgrade the request. Should be used to specify the kind of upgrade i.e. `'Websocket'`. Default: `method === 'CONNECT' || null`. */
    upgrade?: boolean | string | null;
    /** The amount of time the parser will wait to receive the complete HTTP headers. Defaults to 30 seconds. */
    headersTimeout?: number | null;
    /** The timeout after which a request will time out, in milliseconds. Monitors time between receiving body data. Use 0 to disable it entirely. Defaults to 30 seconds. */
    bodyTimeout?: number | null;
    /** Whether Undici should throw an error upon receiving a 4xx or 5xx response from the server. Defaults to false */
    throwOnError?: boolean;
  }
  export interface ConnectOptions {
    path: string;
    /** Default: `null` */
    headers?: IncomingHttpHeaders | string[] | null;
    /** Default: `null` */
    signal?: AbortSignal | EventEmitter | null;
    /** This argument parameter is passed through to `ConnectData` */
    opaque?: unknown;
    /** Default: 0 */
    maxRedirections?: number;
    /** Default: `null` */
    responseHeader?: 'raw' | null;
  }
  export interface RequestOptions extends DispatchOptions {
    /** Default: `null` */
    opaque?: unknown;
    /** Default: `null` */
    signal?: AbortSignal | EventEmitter | null;
    /** Default: 0 */
    maxRedirections?: number;
    /** Default: `null` */
    onInfo?: (info: { statusCode: number, headers: Record<string, string | string[]> }) => void;
    /** Default: `null` */
    responseHeader?: 'raw' | null;
  }
  export interface PipelineOptions extends RequestOptions {
    /** `true` if the `handler` will return an object stream. Default: `false` */
    objectMode?: boolean;
  }
  export interface UpgradeOptions {
    path: string;
    /** Default: `'GET'` */
    method?: string;
    /** Default: `null` */
    headers?: IncomingHttpHeaders | string[] | null;
    /** A string of comma separated protocols, in descending preference order. Default: `'Websocket'` */
    protocol?: string;
    /** Default: `null` */
    signal?: AbortSignal | EventEmitter | null;
    /** Default: 0 */
    maxRedirections?: number;
    /** Default: `null` */
    responseHeader?: 'raw' | null;
  }
  export interface ConnectData {
    statusCode: number;
    headers: IncomingHttpHeaders;
    socket: Duplex;
    opaque: unknown;
  }
  export interface ResponseData {
    statusCode: number;
    headers: IncomingHttpHeaders;
    body: BodyReadable & BodyMixin;
    trailers: Record<string, string>;
    opaque: unknown;
    context: object;
  }
  export interface PipelineHandlerData {
    statusCode: number;
    headers: IncomingHttpHeaders;
    opaque: unknown;
    body: BodyReadable;
    context: object;
  }
  export interface StreamData {
    opaque: unknown;
    trailers: Record<string, string>;
  }
  export interface UpgradeData {
    headers: IncomingHttpHeaders;
    socket: Duplex;
    opaque: unknown;
  }
  export interface StreamFactoryData {
    statusCode: number;
    headers: IncomingHttpHeaders;
    opaque: unknown;
    context: object;
  }
  export type StreamFactory = (data: StreamFactoryData) => Writable;
  export interface DispatchHandlers {
    /** Invoked before request is dispatched on socket. May be invoked multiple times when a request is retried when the request at the head of the pipeline fails. */
    onConnect?(abort: () => void): void;
    /** Invoked when an error has occurred. */
    onError?(err: Error): void;
    /** Invoked when request is upgraded either due to a `Upgrade` header or `CONNECT` method. */
    onUpgrade?(statusCode: number, headers: string[] | null, socket: Duplex): void;
    /** Invoked when statusCode and headers have been received. May be invoked multiple times due to 1xx informational headers. */
    onHeaders?(statusCode: number, headers: string[] | null, resume: () => void): boolean;
    /** Invoked when response payload data is received. */
    onData?(chunk: Buffer): boolean;
    /** Invoked when response payload and trailers have been received and the request has completed. */
    onComplete?(trailers: string[] | null): void;
    /** Invoked when a body chunk is sent to the server. May be invoked multiple times for chunked requests */
    onBodySent?(chunkSize: number, totalBytesSent: number): void;
  }
  export type PipelineHandler = (data: PipelineHandlerData) => Readable;
  export type HttpMethod = 'GET' | 'HEAD' | 'POST' | 'PUT' | 'DELETE' | 'CONNECT' | 'OPTIONS' | 'TRACE' | 'PATCH';

  /**
   * @link https://fetch.spec.whatwg.org/#body-mixin
   */
  interface BodyMixin {
    readonly body?: never; // throws on node v16.6.0
    readonly bodyUsed: boolean;
    arrayBuffer(): Promise<ArrayBuffer>;
    blob(): Promise<Blob>;
    formData(): Promise<never>;
    json(): Promise<any>;
    text(): Promise<string>;
  }
}
