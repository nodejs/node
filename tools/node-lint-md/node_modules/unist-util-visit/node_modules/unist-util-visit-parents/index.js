/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('unist-util-is').Test} Test
 */

/**
 * @typedef {CONTINUE|SKIP|EXIT} Action Union of the action types
 * @typedef {number} Index Move to the sibling at index next (after node itself is completely traversed). Useful if mutating the tree, such as removing the node the visitor is currently on, or any of its previous siblings (or next siblings, in case of reverse) Results less than 0 or greater than or equal to children.length stop traversing the parent
 * @typedef {[(Action|null|undefined|void)?, (Index|null|undefined)?]} ActionTuple List with one or two values, the first an action, the second an index.
 * @typedef {null|undefined|Action|Index|ActionTuple|void} VisitorResult Any value that can be returned from a visitor
 */

/**
 * Invoked when a node (matching test, if given) is found.
 * Visitors are free to transform node.
 * They can also transform the parent of node (the last of ancestors).
 * Replacing node itself, if `SKIP` is not returned, still causes its descendants to be visited.
 * If adding or removing previous siblings (or next siblings, in case of reverse) of node,
 * visitor should return a new index (number) to specify the sibling to traverse after node is traversed.
 * Adding or removing next siblings of node (or previous siblings, in case of reverse)
 * is handled as expected without needing to return a new index.
 * Removing the children property of an ancestor still results in them being traversed.
 *
 * @template {Node} V
 * @callback Visitor
 * @param {V} node Found node
 * @param {Array.<Parent>} ancestors Ancestors of node
 * @returns {VisitorResult}
 */

import {convert} from 'unist-util-is'
import {color} from './color.js'

/**
 * Continue traversing as normal
 */
export const CONTINUE = true
/**
 * Do not traverse this node’s children
 */
export const SKIP = 'skip'
/**
 * Stop traversing immediately
 */
export const EXIT = false

/**
 * Visit children of tree which pass a test
 *
 * @param tree Abstract syntax tree to walk
 * @param test Test node, optional
 * @param visitor Function to run for each node
 * @param reverse Visit the tree in reverse order, defaults to false
 */
export const visitParents =
  /**
   * @type {(
   *   (<Tree extends Node, Check extends Test>(tree: Tree, test: Check, visitor: Visitor<import('./complex-types').Matches<import('./complex-types').InclusiveDescendant<Tree>, Check>>, reverse?: boolean) => void) &
   *   (<Tree extends Node>(tree: Tree, visitor: Visitor<import('./complex-types').InclusiveDescendant<Tree>>, reverse?: boolean) => void)
   * )}
   */
  (
    /**
     * @param {Node} tree
     * @param {Test} test
     * @param {Visitor<Node>} visitor
     * @param {boolean} [reverse]
     */
    function (tree, test, visitor, reverse) {
      if (typeof test === 'function' && typeof visitor !== 'function') {
        reverse = visitor
        // @ts-expect-error no visitor given, so `visitor` is test.
        visitor = test
        test = null
      }

      const is = convert(test)
      const step = reverse ? -1 : 1

      factory(tree, null, [])()

      /**
       * @param {Node} node
       * @param {number?} index
       * @param {Array.<Parent>} parents
       */
      function factory(node, index, parents) {
        /** @type {Object.<string, unknown>} */
        // @ts-expect-error: hush
        const value = typeof node === 'object' && node !== null ? node : {}
        /** @type {string|undefined} */
        let name

        if (typeof value.type === 'string') {
          name =
            typeof value.tagName === 'string'
              ? value.tagName
              : typeof value.name === 'string'
              ? value.name
              : undefined

          Object.defineProperty(visit, 'name', {
            value:
              'node (' +
              color(value.type + (name ? '<' + name + '>' : '')) +
              ')'
          })
        }

        return visit

        function visit() {
          /** @type {ActionTuple} */
          let result = []
          /** @type {ActionTuple} */
          let subresult
          /** @type {number} */
          let offset
          /** @type {Array.<Parent>} */
          let grandparents

          if (!test || is(node, index, parents[parents.length - 1] || null)) {
            result = toResult(visitor(node, parents))

            if (result[0] === EXIT) {
              return result
            }
          }

          // @ts-expect-error looks like a parent.
          if (node.children && result[0] !== SKIP) {
            // @ts-expect-error looks like a parent.
            offset = (reverse ? node.children.length : -1) + step
            // @ts-expect-error looks like a parent.
            grandparents = parents.concat(node)

            // @ts-expect-error looks like a parent.
            while (offset > -1 && offset < node.children.length) {
              // @ts-expect-error looks like a parent.
              subresult = factory(node.children[offset], offset, grandparents)()

              if (subresult[0] === EXIT) {
                return subresult
              }

              offset =
                typeof subresult[1] === 'number' ? subresult[1] : offset + step
            }
          }

          return result
        }
      }
    }
  )

/**
 * @param {VisitorResult} value
 * @returns {ActionTuple}
 */
function toResult(value) {
  if (Array.isArray(value)) {
    return value
  }

  if (typeof value === 'number') {
    return [CONTINUE, value]
  }

  return [value]
}
