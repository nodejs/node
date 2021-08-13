/**
 * @typedef {import('unist').Parent} UnistParent
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Content} Content
 * @typedef {Root|Content} Node
 * @typedef {Extract<Node, UnistParent>} Parent
 */

/**
 * @typedef SafeOptions
 * @property {string} before
 * @property {string} after
 */

/**
 * @callback Enter
 * @param {string} type
 * @returns {Exit}
 */

/**
 * @callback Exit
 * @returns {void}
 */

/**
 * @typedef Context
 * @property {string[]} stack
 *   Stack of labels.
 * @property {number[]} indexStack
 *   Positions of children in their parents.
 * @property {Enter} enter
 * @property {Options} options
 * @property {Array.<Unsafe>} unsafe
 * @property {Array.<Join>} join
 * @property {Handle} handle
 * @property {Handlers} handlers
 * @property {string|undefined} bulletCurrent
 *   The marker used by the current list.
 * @property {string|undefined} bulletLastUsed
 *   The marker used by the previous list.
 */

/**
 * @callback Handle
 * @param {any} node
 * @param {Parent|null|undefined} parent
 * @param {Context} context
 * @param {SafeOptions} safeOptions
 * @returns {string}
 */

/**
 * @typedef {Record<string, Handle>} Handlers
 */

/**
 * @callback Join
 * @param {Node} left
 * @param {Node} right
 * @param {Parent} parent
 * @param {Context} context
 * @returns {boolean|null|void|number}
 */

/**
 * @typedef Unsafe
 * @property {string} character
 * @property {string|Array.<string>} [inConstruct]
 * @property {string|Array.<string>} [notInConstruct]
 * @property {string} [after]
 * @property {string} [before]
 * @property {boolean} [atBreak]
 * @property {RegExp} [_compiled]
 *   The unsafe pattern compiled as a regex
 */

/**
 * @typedef Options
 * @property {'-'|'*'|'+'} [bullet]
 * @property {'-'|'*'|'+'} [bulletOther]
 * @property {'.'|')'} [bulletOrdered]
 * @property {'.'|')'} [bulletOrderedOther]
 * @property {boolean} [closeAtx]
 * @property {'_'|'*'} [emphasis]
 * @property {'~'|'`'} [fence]
 * @property {boolean} [fences]
 * @property {boolean} [incrementListMarker]
 * @property {'tab'|'one'|'mixed'} [listItemIndent]
 * @property {'"'|"'"} [quote]
 * @property {boolean} [resourceLink]
 * @property {'-'|'_'|'*'} [rule]
 * @property {number} [ruleRepetition]
 * @property {boolean} [ruleSpaces]
 * @property {boolean} [setext]
 * @property {'_'|'*'} [strong]
 * @property {boolean} [tightDefinitions]
 * @property {Array.<Options>} [extensions]
 * @property {Handlers} [handlers]
 * @property {Array.<Join>} [join]
 * @property {Array.<Unsafe>} [unsafe]
 */

export {}
