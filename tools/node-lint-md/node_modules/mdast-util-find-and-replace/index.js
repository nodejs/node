/**
 * @typedef Options Configuration.
 * @property {Test} [ignore] `unist-util-is` test used to assert parents
 *
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Content} Content
 * @typedef {import('mdast').PhrasingContent} PhrasingContent
 * @typedef {import('mdast').Text} Text
 * @typedef {Content|Root} Node
 * @typedef {Extract<Node, import('mdast').Parent>} Parent
 *
 * @typedef {import('unist-util-visit-parents').Test} Test
 * @typedef {import('unist-util-visit-parents').VisitorResult} VisitorResult
 *
 * @typedef RegExpMatchObject
 * @property {number} index
 * @property {string} input
 *
 * @typedef {string|RegExp} Find
 * @typedef {string|ReplaceFunction} Replace
 *
 * @typedef {[Find, Replace]} FindAndReplaceTuple
 * @typedef {Object.<string, Replace>} FindAndReplaceSchema
 * @typedef {Array.<FindAndReplaceTuple>} FindAndReplaceList
 *
 * @typedef {[RegExp, ReplaceFunction]} Pair
 * @typedef {Array.<Pair>} Pairs
 */

/**
 * @callback ReplaceFunction
 * @param {...any} parameters
 * @returns {Array.<PhrasingContent>|PhrasingContent|string|false|undefined|null}
 */

import escape from 'escape-string-regexp'
import {visitParents} from 'unist-util-visit-parents'
import {convert} from 'unist-util-is'

const own = {}.hasOwnProperty

/**
 * @param tree mdast tree
 * @param find Value to find and remove. When `string`, escaped and made into a global `RegExp`
 * @param [replace] Value to insert.
 *   * When `string`, turned into a Text node.
 *   * When `Function`, called with the results of calling `RegExp.exec` as
 *     arguments, in which case it can return a single or a list of `Node`,
 *     a `string` (which is wrapped in a `Text` node), or `false` to not replace
 * @param [options] Configuration.
 */
export const findAndReplace =
  /**
   * @type {(
   *   ((tree: Node, find: Find, replace?: Replace, options?: Options) => Node) &
   *   ((tree: Node, schema: FindAndReplaceSchema|FindAndReplaceList, options?: Options) => Node)
   * )}
   **/
  (
    /**
     * @param {Node} tree
     * @param {Find|FindAndReplaceSchema|FindAndReplaceList} find
     * @param {Replace|Options} [replace]
     * @param {Options} [options]
     */
    function (tree, find, replace, options) {
      /** @type {Options|undefined} */
      let settings
      /** @type {FindAndReplaceSchema|FindAndReplaceList} */
      let schema

      if (typeof find === 'string' || find instanceof RegExp) {
        // @ts-expect-error don’t expect options twice.
        schema = [[find, replace]]
        settings = options
      } else {
        schema = find
        // @ts-expect-error don’t expect replace twice.
        settings = replace
      }

      if (!settings) {
        settings = {}
      }

      const ignored = convert(settings.ignore || [])
      const pairs = toPairs(schema)
      let pairIndex = -1

      while (++pairIndex < pairs.length) {
        visitParents(tree, 'text', visitor)
      }

      return tree

      /** @type {import('unist-util-visit-parents').Visitor<Text>} */
      function visitor(node, parents) {
        let index = -1
        /** @type {Parent|undefined} */
        let grandparent

        while (++index < parents.length) {
          const parent = /** @type {Parent} */ (parents[index])

          if (
            ignored(
              parent,
              // @ts-expect-error mdast vs. unist parent.
              grandparent ? grandparent.children.indexOf(parent) : undefined,
              grandparent
            )
          ) {
            return
          }

          grandparent = parent
        }

        if (grandparent) {
          return handler(node, grandparent)
        }
      }

      /**
       * @param {Text} node
       * @param {Parent} parent
       * @returns {VisitorResult}
       */
      function handler(node, parent) {
        const find = pairs[pairIndex][0]
        const replace = pairs[pairIndex][1]
        let start = 0
        // @ts-expect-error: TS is wrong, some of these children can be text.
        let index = parent.children.indexOf(node)
        /** @type {Array.<PhrasingContent>} */
        let nodes = []
        /** @type {number|undefined} */
        let position

        find.lastIndex = 0

        let match = find.exec(node.value)

        while (match) {
          position = match.index
          // @ts-expect-error this is perfectly fine, typescript.
          let value = replace(...match, {
            index: match.index,
            input: match.input
          })

          if (typeof value === 'string') {
            value = value.length > 0 ? {type: 'text', value} : undefined
          }

          if (value !== false) {
            if (start !== position) {
              nodes.push({
                type: 'text',
                value: node.value.slice(start, position)
              })
            }

            if (Array.isArray(value)) {
              nodes.push(...value)
            } else if (value) {
              nodes.push(value)
            }

            start = position + match[0].length
          }

          if (!find.global) {
            break
          }

          match = find.exec(node.value)
        }

        if (position === undefined) {
          nodes = [node]
          index--
        } else {
          if (start < node.value.length) {
            nodes.push({type: 'text', value: node.value.slice(start)})
          }

          parent.children.splice(index, 1, ...nodes)
        }

        return index + nodes.length + 1
      }
    }
  )

/**
 * @param {FindAndReplaceSchema|FindAndReplaceList} schema
 * @returns {Pairs}
 */
function toPairs(schema) {
  /** @type {Pairs} */
  const result = []

  if (typeof schema !== 'object') {
    throw new TypeError('Expected array or object as schema')
  }

  if (Array.isArray(schema)) {
    let index = -1

    while (++index < schema.length) {
      result.push([
        toExpression(schema[index][0]),
        toFunction(schema[index][1])
      ])
    }
  } else {
    /** @type {string} */
    let key

    for (key in schema) {
      if (own.call(schema, key)) {
        result.push([toExpression(key), toFunction(schema[key])])
      }
    }
  }

  return result
}

/**
 * @param {Find} find
 * @returns {RegExp}
 */
function toExpression(find) {
  return typeof find === 'string' ? new RegExp(escape(find), 'g') : find
}

/**
 * @param {Replace} replace
 * @returns {ReplaceFunction}
 */
function toFunction(replace) {
  return typeof replace === 'function' ? replace : () => replace
}
