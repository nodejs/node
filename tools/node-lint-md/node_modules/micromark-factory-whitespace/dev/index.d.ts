/**
 * @param {Effects} effects
 * @param {State} ok
 */
export function factoryWhitespace(
  effects: Effects,
  ok: State
): (
  code: import('micromark-util-types').Code
) => void | import('micromark-util-types').State
export type Effects = import('micromark-util-types').Effects
export type State = import('micromark-util-types').State
