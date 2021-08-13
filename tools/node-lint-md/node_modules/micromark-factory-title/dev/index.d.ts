/**
 * @param {Effects} effects
 * @param {State} ok
 * @param {State} nok
 * @param {string} type
 * @param {string} markerType
 * @param {string} stringType
 * @returns {State}
 */
export function factoryTitle(
  effects: Effects,
  ok: State,
  nok: State,
  type: string,
  markerType: string,
  stringType: string
): State
export type Effects = import('micromark-util-types').Effects
export type State = import('micromark-util-types').State
export type Code = import('micromark-util-types').Code
