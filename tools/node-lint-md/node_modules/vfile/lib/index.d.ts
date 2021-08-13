export class VFile {
  /**
   * Create a new virtual file.
   *
   * If `options` is `string` or `Buffer`, treats it as `{value: options}`.
   * If `options` is a `VFile`, shallow copies its data over to the new file.
   * All other given fields are set on the newly created `VFile`.
   *
   * Path related properties are set in the following order (least specific to
   * most specific): `history`, `path`, `basename`, `stem`, `extname`,
   * `dirname`.
   *
   * It’s not possible to set either `dirname` or `extname` without setting
   * either `history`, `path`, `basename`, or `stem` as well.
   *
   * @param {VFileCompatible} [value]
   */
  constructor(value?: VFileCompatible | undefined)
  /**
   * Place to store custom information.
   * It’s OK to store custom data directly on the file, moving it to `data`
   * gives a little more privacy.
   * @type {Object.<string, unknown>}
   */
  data: {
    [x: string]: unknown
  }
  /**
   * List of messages associated with the file.
   * @type {Array.<VFileMessage>}
   */
  messages: Array<VFileMessage>
  /**
   * List of file paths the file moved between.
   * @type {Array.<string>}
   */
  history: Array<string>
  /**
   * Base of `path`.
   * Defaults to `process.cwd()` (`/` in browsers).
   * @type {string}
   */
  cwd: string
  /**
   * Raw value.
   * @type {VFileValue}
   */
  value: VFileValue
  /**
   * Whether a file was saved to disk.
   * This is used by vfile reporters.
   * @type {boolean}
   */
  stored: boolean
  /**
   * Sometimes files have a non-string representation.
   * This can be stored in the `result` field.
   * One example is when turning markdown into React nodes.
   * This is used by unified to store non-string results.
   * @type {unknown}
   */
  result: unknown
  /**
   * Sometimes files have a source map associated with them.
   * This can be stored in the `map` field.
   * This should be a `RawSourceMap` type from the `source-map` module.
   * @type {unknown}
   */
  map: unknown
  /**
   * Set full path (`~/index.min.js`).
   * Cannot be nullified.
   *
   * @param {string|URL} path
   */
  set path(arg: string)
  /**
   * Access full path (`~/index.min.js`).
   *
   * @returns {string}
   */
  get path(): string
  /**
   * Set parent path (`~`).
   * Cannot be set if there's no `path` yet.
   */
  set dirname(arg: string | undefined)
  /**
   * Access parent path (`~`).
   */
  get dirname(): string | undefined
  /**
   * Set basename (`index.min.js`).
   * Cannot contain path separators.
   * Cannot be nullified either (use `file.path = file.dirname` instead).
   */
  set basename(arg: string | undefined)
  /**
   * Access basename (including extname) (`index.min.js`).
   */
  get basename(): string | undefined
  /**
   * Set extname (including dot) (`.js`).
   * Cannot be set if there's no `path` yet and cannot contain path separators.
   */
  set extname(arg: string | undefined)
  /**
   * Access extname (including dot) (`.js`).
   */
  get extname(): string | undefined
  /**
   * Set stem (w/o extname) (`index.min`).
   * Cannot be nullified, and cannot contain path separators.
   */
  set stem(arg: string | undefined)
  /**
   * Access stem (w/o extname) (`index.min`).
   */
  get stem(): string | undefined
  /**
   * Serialize the file.
   *
   * @param {BufferEncoding} [encoding='utf8'] If `file.value` is a buffer, `encoding` is used to serialize buffers.
   * @returns {string}
   */
  toString(encoding?: BufferEncoding | undefined): string
  /**
   * Create a message and associates it w/ the file.
   *
   * @param {string|Error} reason Reason for message (`string` or `Error`). Uses the stack and message of the error if given.
   * @param {Node|Position|Point} [place] Place at which the message occurred in a file (`Node`, `Position`, or `Point`, optional).
   * @param {string} [origin] Place in code the message originates from (`string`, optional).
   * @returns {VFileMessage}
   */
  message(
    reason: string | Error,
    place?:
      | import('unist').Node<import('unist').Data>
      | import('unist').Position
      | import('unist').Point
      | undefined,
    origin?: string | undefined
  ): VFileMessage
  /**
   * Info: create a message, associate it with the file, and mark the fatality
   * as `null`.
   * Calls `message()` internally.
   *
   * @param {string|Error} reason Reason for message (`string` or `Error`). Uses the stack and message of the error if given.
   * @param {Node|Position|Point} [place] Place at which the message occurred in a file (`Node`, `Position`, or `Point`, optional).
   * @param {string} [origin] Place in code the message originates from (`string`, optional).
   * @returns {VFileMessage}
   */
  info(
    reason: string | Error,
    place?:
      | import('unist').Node<import('unist').Data>
      | import('unist').Position
      | import('unist').Point
      | undefined,
    origin?: string | undefined
  ): VFileMessage
  /**
   * Fail: create a message, associate it with the file, mark the fatality as
   * `true`.
   * Note: fatal errors mean a file is no longer processable.
   * Calls `message()` internally.
   *
   * @param {string|Error} reason Reason for message (`string` or `Error`). Uses the stack and message of the error if given.
   * @param {Node|Position|Point} [place] Place at which the message occurred in a file (`Node`, `Position`, or `Point`, optional).
   * @param {string} [origin] Place in code the message originates from (`string`, optional).
   * @returns {never}
   */
  fail(
    reason: string | Error,
    place?:
      | import('unist').Node<import('unist').Data>
      | import('unist').Position
      | import('unist').Point
      | undefined,
    origin?: string | undefined
  ): never
}
export type Node = import('unist').Node
export type Position = import('unist').Position
export type Point = import('unist').Point
export type URL = import('./minurl.shared.js').URL
/**
 * Encodings supported by the buffer class.
 * This is a copy of the typing from Node, copied to prevent Node globals from
 * being needed.
 * Copied from: <https://github.com/DefinitelyTyped/DefinitelyTyped/blob/a2bc1d8/types/node/globals.d.ts#L174>
 */
export type BufferEncoding =
  | 'ascii'
  | 'utf8'
  | 'utf-8'
  | 'utf16le'
  | 'ucs2'
  | 'ucs-2'
  | 'base64'
  | 'latin1'
  | 'binary'
  | 'hex'
/**
 * Contents of the file.
 * Can either be text, or a Buffer like structure.
 * This does not directly use type `Buffer`, because it can also be used in a
 * browser context.
 * Instead this leverages `Uint8Array` which is the base type for `Buffer`,
 * and a native JavaScript construct.
 */
export type VFileValue = string | Uint8Array
/**
 * Things that can be passed to the constructor.
 */
export type VFileCompatible = VFileValue | VFileOptions | VFile | URL
export type VFileCoreOptions = {
  value?: VFileValue | undefined
  cwd?: string | undefined
  history?: string[] | undefined
  path?: string | import('./minurl.shared.js').URL | undefined
  basename?: string | undefined
  stem?: string | undefined
  extname?: string | undefined
  dirname?: string | undefined
  data?:
    | {
        [x: string]: unknown
      }
    | undefined
}
/**
 * Configuration: a bunch of keys that will be shallow copied over to the new
 * file.
 */
export type VFileOptions = {
  [key: string]: unknown
} & VFileCoreOptions
export type VFileReporterSettings = {
  [x: string]: unknown
}
export type VFileReporter = <
  T = {
    [x: string]: unknown
  }
>(
  files: VFile[],
  options: T
) => string
import {VFileMessage} from 'vfile-message'
