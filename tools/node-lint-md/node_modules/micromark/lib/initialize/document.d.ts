/** @type {InitialConstruct} */
export const document: InitialConstruct
export type InitialConstruct = import('micromark-util-types').InitialConstruct
export type Initializer = import('micromark-util-types').Initializer
export type Construct = import('micromark-util-types').Construct
export type TokenizeContext = import('micromark-util-types').TokenizeContext
export type Tokenizer = import('micromark-util-types').Tokenizer
export type Token = import('micromark-util-types').Token
export type State = import('micromark-util-types').State
export type Point = import('micromark-util-types').Point
export type StackState = Record<string, unknown>
export type StackItem = [Construct, StackState]
