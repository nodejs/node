/**
 * @this {TokenizeContext}
 * @param {Effects} effects
 * @param {State} ok
 * @param {State} nok
 * @param {string} type
 * @param {string} markerType
 * @param {string} stringType
 * @returns {State}
 */
export function factoryLabel(
  effects: Effects,
  ok: State,
  nok: State,
  type: string,
  markerType: string,
  stringType: string
): State
export type Effects = import('micromark-util-types').Effects
export type TokenizeContext = import('micromark-util-types').TokenizeContext
export type State = import('micromark-util-types').State
