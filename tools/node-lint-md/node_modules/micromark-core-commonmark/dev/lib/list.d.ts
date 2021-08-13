/** @type {Construct} */
export const list: Construct
export type Construct = import('micromark-util-types').Construct
export type TokenizeContext = import('micromark-util-types').TokenizeContext
export type Exiter = import('micromark-util-types').Exiter
export type Tokenizer = import('micromark-util-types').Tokenizer
export type State = import('micromark-util-types').State
export type Code = import('micromark-util-types').Code
export type ListContainerState = Record<string, unknown> & {
  marker: Code
  type: string
  size: number
}
export type TokenizeContextWithState = TokenizeContext & {
  containerState: ListContainerState
}
