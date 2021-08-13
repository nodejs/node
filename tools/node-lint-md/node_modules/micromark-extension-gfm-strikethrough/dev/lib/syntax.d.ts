/**
 * @param {Options} [options]
 * @returns {Extension}
 */
export function gfmStrikethrough(options?: Options | undefined): Extension
export type Extension = import('micromark-util-types').Extension
export type Resolver = import('micromark-util-types').Resolver
export type Tokenizer = import('micromark-util-types').Tokenizer
export type State = import('micromark-util-types').State
export type Token = import('micromark-util-types').Token
export type Event = import('micromark-util-types').Event
export type Options = {
  /**
   * Whether to support strikethrough with a single tilde (`boolean`, default:
   * `true`).
   * Single tildes work on github.com, but are technically prohibited by the
   * GFM spec.
   */
  singleTilde?: boolean | undefined
}
