/**
 * @param {Effects} effects
 * @param {State} ok
 * @param {State} nok
 * @param {string} type
 * @param {string} literalType
 * @param {string} literalMarkerType
 * @param {string} rawType
 * @param {string} stringType
 * @param {number} [max=Infinity]
 * @returns {State}
 */
export function factoryDestination(
  effects: Effects,
  ok: State,
  nok: State,
  type: string,
  literalType: string,
  literalMarkerType: string,
  rawType: string,
  stringType: string,
  max?: number | undefined
): State
export type Effects = import('micromark-util-types').Effects
export type State = import('micromark-util-types').State
