/**
 * @typedef {import('../types.js').Node} Node
 * @typedef {import('../types.js').Parent} Parent
 * @typedef {import('../types.js').Join} Join
 * @typedef {import('../types.js').Context} Context
 */

/**
 * @param {Parent} parent
 * @param {Context} context
 * @returns {string}
 */
export function containerFlow(parent, context) {
  const indexStack = context.indexStack
  const children = parent.children || []
  /** @type {Array.<string>} */
  const results = []
  let index = -1

  indexStack.push(-1)

  while (++index < children.length) {
    const child = children[index]

    indexStack[indexStack.length - 1] = index

    results.push(
      context.handle(child, parent, context, {before: '\n', after: '\n'})
    )

    if (child.type !== 'list') {
      context.bulletLastUsed = undefined
    }

    if (index < children.length - 1) {
      results.push(between(child, children[index + 1]))
    }
  }

  indexStack.pop()

  return results.join('')

  /**
   * @param {Node} left
   * @param {Node} right
   * @returns {string}
   */
  function between(left, right) {
    let index = context.join.length

    while (index--) {
      const result = context.join[index](left, right, parent, context)

      if (result === true || result === 1) {
        break
      }

      if (typeof result === 'number') {
        return '\n'.repeat(1 + result)
      }

      if (result === false) {
        return '\n\n<!---->\n\n'
      }
    }

    return '\n\n'
  }
}
