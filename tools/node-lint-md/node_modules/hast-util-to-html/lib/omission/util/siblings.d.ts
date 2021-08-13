/**
 * Find applicable siblings in a direction.
 *
 * @param {Parent} parent
 * @param {number} index
 * @param {boolean} [includeWhitespace=false]
 * @returns {Child}
 */
export function siblingAfter(
  parent: Parent,
  index: number,
  includeWhitespace?: boolean
): Child
/**
 * Find applicable siblings in a direction.
 *
 * @param {Parent} parent
 * @param {number} index
 * @param {boolean} [includeWhitespace=false]
 * @returns {Child}
 */
export function siblingBefore(
  parent: Parent,
  index: number,
  includeWhitespace?: boolean
): Child
export type Parent = import('../../types.js').Parent
export type Child = import('../../types.js').Child
