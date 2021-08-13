/**
 * @param {Effects} effects
 * @param {State} ok
 * @param {string} type
 * @param {number} [max=Infinity]
 * @returns {State}
 */
export function factorySpace(
  effects: Effects,
  ok: State,
  type: string,
  max?: number | undefined
): State
export type Effects = import('micromark-util-types').Effects
export type State = import('micromark-util-types').State
