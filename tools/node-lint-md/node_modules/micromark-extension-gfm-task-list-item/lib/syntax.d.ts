export namespace gfmTaskListItem {
  const text: {
    [x: number]: {
      tokenize: typeof tokenizeTasklistCheck
    }
  }
}
export type Extension = import('micromark-util-types').Extension
export type ConstructRecord = import('micromark-util-types').ConstructRecord
export type Tokenizer = import('micromark-util-types').Tokenizer
export type Previous = import('micromark-util-types').Previous
export type State = import('micromark-util-types').State
export type Event = import('micromark-util-types').Event
export type Code = import('micromark-util-types').Code
/** @type {Tokenizer} */
declare function tokenizeTasklistCheck(
  effects: import('micromark-util-types').Effects,
  ok: import('micromark-util-types').State,
  nok: import('micromark-util-types').State
): import('micromark-util-types').State
export {}
