/**
 * Create a tokenizer.
 * Tokenizers deal with one type of data (e.g., containers, flow, text).
 * The parser is the object dealing with it all.
 * `initialize` works like other constructs, except that only its `tokenize`
 * function is used, in which case it doesn’t receive an `ok` or `nok`.
 * `from` can be given to set the point before the first character, although
 * when further lines are indented, they must be set with `defineSkip`.
 *
 * @param {ParseContext} parser
 * @param {InitialConstruct} initialize
 * @param {Omit<Point, '_index'|'_bufferIndex'>} [from]
 * @returns {TokenizeContext}
 */
export function createTokenizer(
  parser: ParseContext,
  initialize: InitialConstruct,
  from?:
    | Omit<import('micromark-util-types').Point, '_index' | '_bufferIndex'>
    | undefined
): TokenizeContext
export type Code = import('micromark-util-types').Code
export type Chunk = import('micromark-util-types').Chunk
export type Point = import('micromark-util-types').Point
export type Token = import('micromark-util-types').Token
export type Effects = import('micromark-util-types').Effects
export type State = import('micromark-util-types').State
export type Construct = import('micromark-util-types').Construct
export type InitialConstruct = import('micromark-util-types').InitialConstruct
export type ConstructRecord = import('micromark-util-types').ConstructRecord
export type TokenizeContext = import('micromark-util-types').TokenizeContext
export type ParseContext = import('micromark-util-types').ParseContext
export type Info = {
  restore: () => void
  from: number
}
/**
 * Handle a successful run.
 */
export type ReturnHandle = (construct: Construct, info: Info) => void
