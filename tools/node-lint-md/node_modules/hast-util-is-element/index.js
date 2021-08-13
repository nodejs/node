/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('hast').Element} Element
 *
 * @typedef {string} TagName
 * @typedef {null|undefined|TagName|TestFunctionAnything|Array.<TagName|TestFunctionAnything>} Test
 */

/**
 * Check if an element passes a test
 *
 * @callback TestFunctionAnything
 * @param {Element} element
 * @param {number|null|undefined} [index]
 * @param {Parent|null|undefined} [parent]
 * @returns {boolean|void}
 */

/**
 * Check if an element passes a certain node test
 *
 * @template {Element} X
 * @callback TestFunctionPredicate
 * @param {X} element
 * @param {number|null|undefined} [index]
 * @param {Parent|null|undefined} [parent]
 * @returns {element is X}
 */

/**
 * Check if a node is an element and passes a certain node test
 *
 * @callback AssertAnything
 * @param {unknown} [node]
 * @param {number|null|undefined} [index]
 * @param {Parent|null|undefined} [parent]
 * @returns {boolean}
 */

/**
 * Check if a node is an element and passes a certain node test
 *
 * @template {Element} Y
 * @callback AssertPredicate
 * @param {unknown} [node]
 * @param {number|null|undefined} [index]
 * @param {Parent|null|undefined} [parent]
 * @returns {node is Y}
 */

// Check if `node` is an `element` and whether it passes the given test.
export const isElement =
  /**
   * Check if a node is an element and passes a test.
   * When a `parent` node is known the `index` of node should also be given.
   *
   * @type {(
   *   (<T extends Element>(node: unknown, test: T['tagName']|TestFunctionPredicate<T>|Array.<T['tagName']|TestFunctionPredicate<T>>, index?: number, parent?: Parent, context?: unknown) => node is T) &
   *   ((node?: unknown, test?: Test, index?: number, parent?: Parent, context?: unknown) => boolean)
   * )}
   */
  (
    /**
     * Check if a node passes a test.
     * When a `parent` node is known the `index` of node should also be given.
     *
     * @param {unknown} [node] Node to check
     * @param {Test} [test] When nullish, checks if `node` is a `Node`.
     * When `string`, works like passing `function (node) {return node.type === test}`.
     * When `function` checks if function passed the node is true.
     * When `array`, checks any one of the subtests pass.
     * @param {number} [index] Position of `node` in `parent`
     * @param {Parent} [parent] Parent of `node`
     * @param {unknown} [context] Context object to invoke `test` with
     * @returns {boolean} Whether test passed and `node` is an `Element` (object with `type` set to `element` and `tagName` set to a non-empty string).
     */
    // eslint-disable-next-line max-params
    function (node, test, index, parent, context) {
      const check = convertElement(test)

      if (
        index !== undefined &&
        index !== null &&
        (typeof index !== 'number' ||
          index < 0 ||
          index === Number.POSITIVE_INFINITY)
      ) {
        throw new Error('Expected positive finite index for child node')
      }

      if (
        parent !== undefined &&
        parent !== null &&
        (!parent.type || !parent.children)
      ) {
        throw new Error('Expected parent node')
      }

      // @ts-expect-error Looks like a node.
      if (!node || !node.type || typeof node.type !== 'string') {
        return false
      }

      if (
        (parent === undefined || parent === null) !==
        (index === undefined || index === null)
      ) {
        throw new Error('Expected both parent and index')
      }

      return check.call(context, node, index, parent)
    }
  )

export const convertElement =
  /**
   * @type {(
   *   (<T extends Element>(test: T['tagName']|TestFunctionPredicate<T>) => AssertPredicate<T>) &
   *   ((test?: Test) => AssertAnything)
   * )}
   */
  (
    /**
     * Generate an assertion from a check.
     * @param {Test} [test]
     * When nullish, checks if `node` is a `Node`.
     * When `string`, works like passing `function (node) {return node.type === test}`.
     * When `function` checks if function passed the node is true.
     * When `object`, checks that all keys in test are in node, and that they have (strictly) equal values.
     * When `array`, checks any one of the subtests pass.
     * @returns {AssertAnything}
     */
    function (test) {
      if (test === undefined || test === null) {
        return element
      }

      if (typeof test === 'string') {
        return tagNameFactory(test)
      }

      if (typeof test === 'object') {
        return anyFactory(test)
      }

      if (typeof test === 'function') {
        return castFactory(test)
      }

      throw new Error('Expected function, string, or array as test')
    }
  )

/**
 * @param {Array.<TagName|TestFunctionAnything>} tests
 * @returns {AssertAnything}
 */
function anyFactory(tests) {
  /** @type {Array.<AssertAnything>} */
  const checks = []
  let index = -1

  while (++index < tests.length) {
    checks[index] = convertElement(tests[index])
  }

  return castFactory(any)

  /**
   * @this {unknown}
   * @param {unknown[]} parameters
   * @returns {boolean}
   */
  function any(...parameters) {
    let index = -1

    while (++index < checks.length) {
      if (checks[index].call(this, ...parameters)) {
        return true
      }
    }

    return false
  }
}

/**
 * Utility to convert a string into a function which checks a given node’s tag
 * name for said string.
 *
 * @param {TagName} check
 * @returns {AssertAnything}
 */
function tagNameFactory(check) {
  return tagName

  /**
   * @param {unknown} node
   * @returns {boolean}
   */
  function tagName(node) {
    return element(node) && node.tagName === check
  }
}

/**
 * @param {TestFunctionAnything} check
 * @returns {AssertAnything}
 */
function castFactory(check) {
  return assertion

  /**
   * @this {unknown}
   * @param {unknown} node
   * @param {Array.<unknown>} parameters
   * @returns {boolean}
   */
  function assertion(node, ...parameters) {
    // @ts-expect-error: fine.
    return element(node) && Boolean(check.call(this, node, ...parameters))
  }
}

/**
 * Utility to return true if this is an element.
 * @param {unknown} node
 * @returns {node is Element}
 */
function element(node) {
  return Boolean(
    node &&
      typeof node === 'object' &&
      // @ts-expect-error Looks like a node.
      node.type === 'element' &&
      // @ts-expect-error Looks like an element.
      typeof node.tagName === 'string'
  )
}
