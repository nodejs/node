// based on https://github.com/Ethan-Arrowood/undici-fetch/blob/249269714db874351589d2d364a0645d5160ae71/index.d.ts (MIT license)
// and https://github.com/node-fetch/node-fetch/blob/914ce6be5ec67a8bab63d68510aabf07cb818b6d/index.d.ts (MIT license)
/// <reference types="node" />

import { Blob } from 'node:buffer'
import { URL, URLSearchParams } from 'node:url'
import { ReadableStream } from 'node:stream/web'
import { FormData } from './formdata'
import { HeaderRecord } from './header'
import Dispatcher from './dispatcher'

export type RequestInfo = string | URL | Request

export declare function fetch (
  input: RequestInfo,
  init?: RequestInit
): Promise<Response>

export type BodyInit =
  | ArrayBuffer
  | AsyncIterable<Uint8Array>
  | Blob
  | FormData
  | Iterable<Uint8Array>
  | NodeJS.ArrayBufferView
  | URLSearchParams
  | null
  | string

export class BodyMixin {
  readonly body: ReadableStream | null
  readonly bodyUsed: boolean

  readonly arrayBuffer: () => Promise<ArrayBuffer>
  readonly blob: () => Promise<Blob>
  readonly bytes: () => Promise<Uint8Array>
  /**
   * @deprecated This method is not recommended for parsing multipart/form-data bodies in server environments.
   * It is recommended to use a library such as [@fastify/busboy](https://www.npmjs.com/package/@fastify/busboy) as follows:
   *
   * @example
   * ```js
   * import { Busboy } from '@fastify/busboy'
   * import { Readable } from 'node:stream'
   *
   * const response = await fetch('...')
   * const busboy = new Busboy({ headers: { 'content-type': response.headers.get('content-type') } })
   *
   * // handle events emitted from `busboy`
   *
   * Readable.fromWeb(response.body).pipe(busboy)
   * ```
   */
  readonly formData: () => Promise<FormData>
  readonly json: () => Promise<unknown>
  readonly text: () => Promise<string>
}

export interface SpecIterator<T, TReturn = any, TNext = undefined> {
  next(...args: [] | [TNext]): IteratorResult<T, TReturn>;
}

export interface SpecIteratorObject<T, TReturn = undefined, TNext = unknown> extends SpecIterator<T, TReturn, TNext> {
  [Symbol.iterator](): SpecIteratorObject<T, TReturn, TNext>;
  map<U>(callbackfn: (value: T, index: number) => U): SpecIteratorObject<U>;
  filter<S extends T>(predicate: (value: T, index: number) => value is S): SpecIteratorObject<S>;
  filter(predicate: (value: T, index: number) => unknown): SpecIteratorObject<T>;
  take(limit: number): SpecIteratorObject<T>;
  drop(count: number): SpecIteratorObject<T>;
  flatMap<U>(callbackfn: (value: T, index: number) => Iterator<U> | Iterable<U>): SpecIteratorObject<U>;
  reduce(callbackfn: (previousValue: T, currentValue: T, currentIndex: number) => T): T;
  reduce(callbackfn: (previousValue: T, currentValue: T, currentIndex: number) => T, initialValue: T): T;
  reduce<U>(callbackfn: (previousValue: U, currentValue: T, currentIndex: number) => U, initialValue: U): U;
  toArray(): T[];
  forEach(callbackfn: (value: T, index: number) => void): void;
  some(predicate: (value: T, index: number) => unknown): boolean;
  every(predicate: (value: T, index: number) => unknown): boolean;
  find<S extends T>(predicate: (value: T, index: number) => value is S): S | undefined;
  find(predicate: (value: T, index: number) => unknown): T | undefined;
  readonly [Symbol.toStringTag]: string;
}

export interface SpecIterableIterator<T> extends SpecIteratorObject<T> {
  [Symbol.iterator](): SpecIterableIterator<T>;
}

export interface SpecIterable<T> {
  [Symbol.iterator](): SpecIterableIterator<T>;
}

export type HeadersInit = [string, string][] | HeaderRecord | Headers

export declare class Headers implements SpecIterable<[string, string]> {
  constructor (init?: HeadersInit)
  readonly append: (name: string, value: string) => void
  readonly delete: (name: string) => void
  readonly get: (name: string) => string | null
  readonly has: (name: string) => boolean
  readonly set: (name: string, value: string) => void
  readonly getSetCookie: () => string[]
  readonly forEach: (
    callbackfn: (value: string, key: string, iterable: Headers) => void,
    thisArg?: unknown
  ) => void

  readonly keys: () => SpecIterableIterator<string>
  readonly values: () => SpecIterableIterator<string>
  readonly entries: () => SpecIterableIterator<[string, string]>
  readonly [Symbol.iterator]: () => SpecIterableIterator<[string, string]>
}

export type RequestCache =
  | 'default'
  | 'force-cache'
  | 'no-cache'
  | 'no-store'
  | 'only-if-cached'
  | 'reload'

export type RequestCredentials = 'omit' | 'include' | 'same-origin'

type RequestDestination =
  | ''
  | 'audio'
  | 'audioworklet'
  | 'document'
  | 'embed'
  | 'font'
  | 'image'
  | 'manifest'
  | 'object'
  | 'paintworklet'
  | 'report'
  | 'script'
  | 'sharedworker'
  | 'style'
  | 'track'
  | 'video'
  | 'worker'
  | 'xslt'

export interface RequestInit {
  body?: BodyInit | null
  cache?: RequestCache
  credentials?: RequestCredentials
  dispatcher?: Dispatcher
  duplex?: RequestDuplex
  headers?: HeadersInit
  integrity?: string
  keepalive?: boolean
  method?: string
  mode?: RequestMode
  redirect?: RequestRedirect
  referrer?: string
  referrerPolicy?: ReferrerPolicy
  signal?: AbortSignal | null
  window?: null
}

export type ReferrerPolicy =
  | ''
  | 'no-referrer'
  | 'no-referrer-when-downgrade'
  | 'origin'
  | 'origin-when-cross-origin'
  | 'same-origin'
  | 'strict-origin'
  | 'strict-origin-when-cross-origin'
  | 'unsafe-url'

export type RequestMode = 'cors' | 'navigate' | 'no-cors' | 'same-origin'

export type RequestRedirect = 'error' | 'follow' | 'manual'

export type RequestDuplex = 'half'

export declare class Request extends BodyMixin {
  constructor (input: RequestInfo, init?: RequestInit)

  readonly cache: RequestCache
  readonly credentials: RequestCredentials
  readonly destination: RequestDestination
  readonly headers: Headers
  readonly integrity: string
  readonly method: string
  readonly mode: RequestMode
  readonly redirect: RequestRedirect
  readonly referrer: string
  readonly referrerPolicy: ReferrerPolicy
  readonly url: string

  readonly keepalive: boolean
  readonly signal: AbortSignal
  readonly duplex: RequestDuplex

  public clone (): Request
}

export interface ResponseInit {
  readonly status?: number
  readonly statusText?: string
  readonly headers?: HeadersInit
}

export type ResponseType =
  | 'basic'
  | 'cors'
  | 'default'
  | 'error'
  | 'opaque'
  | 'opaqueredirect'

export type ResponseRedirectStatus = 301 | 302 | 303 | 307 | 308

export declare class Response extends BodyMixin {
  constructor (body?: BodyInit, init?: ResponseInit)

  readonly headers: Headers
  readonly ok: boolean
  readonly status: number
  readonly statusText: string
  readonly type: ResponseType
  readonly url: string
  readonly redirected: boolean

  public clone (): Response

  static error (): Response
  static json (data: any, init?: ResponseInit): Response
  static redirect (url: string | URL, status?: ResponseRedirectStatus): Response
}
