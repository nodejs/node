/**
 * @typedef {import('unist').Node} Node
 * @typedef {import('unist').Parent} Parent
 * @typedef {import('unist').Point} Point
 * @typedef {import('unist-util-is').Test} Test
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('vfile-message').VFileMessage} VFileMessage
 *
 * @typedef {OptionsWithoutReset|OptionsWithReset} Options
 * @typedef {OptionsBaseFields & OptionsWithoutResetFields} OptionsWithoutReset
 * @typedef {OptionsBaseFields & OptionsWithResetFields} OptionsWithReset
 *
 * @typedef OptionsWithoutResetFields
 * @property {false} [reset]
 *   Whether to treat all messages as turned off initially.
 * @property {string[]} [disable]
 *   List of `ruleId`s to turn off.
 *
 * @typedef OptionsWithResetFields
 * @property {true} reset
 *   Whether to treat all messages as turned off initially.
 * @property {string[]} [enable]
 *   List of `ruleId`s to initially turn on.
 *
 * @typedef OptionsBaseFields
 * @property {string} name
 *   Name of markers that can control the message sources.
 *
 *   For example, `{name: 'alpha'}` controls `alpha` markers:
 *
 *   ```html
 *   <!--alpha ignore-->
 *   ```
 * @property {MarkerParser} marker
 *   Parse a possible marker to a comment marker object (Marker).
 *   If the marker isn't a marker, should return `null`.
 * @property {Test} [test]
 *   Test for possible markers
 * @property {string[]} [known]
 *   List of allowed `ruleId`s. When given a warning is shown
 *   when someone tries to control an unknown rule.
 *
 *   For example, `{name: 'alpha', known: ['bravo']}` results in a warning if
 *   `charlie` is configured:
 *
 *   ```html
 *   <!--alpha ignore charlie-->
 *   ```
 * @property {string|string[]} [source]
 *   Sources that can be controlled with `name` markers.
 *   Defaults to `name`.
 *
 * @callback MarkerParser
 *   Parse a possible comment marker node to a Marker.
 * @param {Node} node
 *   Node to parse
 *
 * @typedef Marker
 *   A comment marker.
 * @property {string} name
 *   Name of marker.
 * @property {string} attributes
 *   Value after name.
 * @property {Record<string, string|number|boolean>} parameters
 *   Parsed attributes.
 * @property {Node} node
 *   Reference to given node.
 *
 * @typedef Mark
 * @property {Point|undefined} point
 * @property {boolean} state
 */

import {location} from 'vfile-location'
import {visit} from 'unist-util-visit'

const own = {}.hasOwnProperty

/**
 * @type {import('unified').Plugin<[Options]>}
 * @returns {(tree: Node, file: VFile) => void}
 */
export default function messageControl(options) {
  if (!options || typeof options !== 'object' || !options.name) {
    throw new Error(
      'Expected `name` in `options`, got `' + (options || {}).name + '`'
    )
  }

  if (!options.marker) {
    throw new Error(
      'Expected `marker` in `options`, got `' + options.marker + '`'
    )
  }

  const enable = 'enable' in options && options.enable ? options.enable : []
  const disable = 'disable' in options && options.disable ? options.disable : []
  let reset = options.reset
  const sources =
    typeof options.source === 'string'
      ? [options.source]
      : options.source || [options.name]

  return transformer

  /**
   * @param {Node} tree
   * @param {VFile} file
   */
  function transformer(tree, file) {
    const toOffset = location(file).toOffset
    const initial = !reset
    const gaps = detectGaps(tree, file)
    /** @type {Record<string, Mark[]>} */
    const scope = {}
    /** @type {Mark[]} */
    const globals = []

    visit(tree, options.test, visitor)

    file.messages = file.messages.filter((m) => filter(m))

    /**
     * @param {Node} node
     * @param {number|null} position
     * @param {Parent|null} parent
     */
    function visitor(node, position, parent) {
      /** @type {Marker|null} */
      const mark = options.marker(node)

      if (!mark || mark.name !== options.name) {
        return
      }

      const ruleIds = mark.attributes.split(/\s/g)
      const point = mark.node.position && mark.node.position.start
      const next =
        (parent && position !== null && parent.children[position + 1]) ||
        undefined
      const tail = (next && next.position && next.position.end) || undefined
      let index = -1

      /** @type {string} */
      // @ts-expect-error: we’ll check for unknown values next.
      const verb = ruleIds.shift()

      if (verb !== 'enable' && verb !== 'disable' && verb !== 'ignore') {
        file.fail(
          'Unknown keyword `' +
            verb +
            '`: expected ' +
            "`'enable'`, `'disable'`, or `'ignore'`",
          mark.node
        )
      }

      // Apply to all rules.
      if (ruleIds.length > 0) {
        while (++index < ruleIds.length) {
          const ruleId = ruleIds[index]

          if (isKnown(ruleId, verb, mark.node)) {
            toggle(point, verb === 'enable', ruleId)

            if (verb === 'ignore') {
              toggle(tail, true, ruleId)
            }
          }
        }
      } else if (verb === 'ignore') {
        toggle(point, false)
        toggle(tail, true)
      } else {
        toggle(point, verb === 'enable')
        reset = verb !== 'enable'
      }
    }

    /**
     * @param {VFileMessage} message
     * @returns {boolean}
     */
    function filter(message) {
      let gapIndex = gaps.length

      // Keep messages from a different source.
      if (!message.source || !sources.includes(message.source)) {
        return true
      }

      // We only ignore messages if they‘re disabled, *not* when they’re not in
      // the document.
      if (!message.line) {
        message.line = 1
      }

      if (!message.column) {
        message.column = 1
      }

      // Check whether the warning is inside a gap.
      // @ts-expect-error: we just normalized `null` to `number`s.
      const offset = toOffset(message)

      while (gapIndex--) {
        if (gaps[gapIndex][0] <= offset && gaps[gapIndex][1] > offset) {
          return false
        }
      }

      // Check whether allowed by specific and global states.
      return (
        (!message.ruleId ||
          check(message, scope[message.ruleId], message.ruleId)) &&
        check(message, globals)
      )
    }

    /**
     * Helper to check (and possibly warn) if a `ruleId` is unknown.
     *
     * @param {string} ruleId
     * @param {string} verb
     * @param {Node} node
     * @returns {boolean}
     */
    function isKnown(ruleId, verb, node) {
      const result = options.known ? options.known.includes(ruleId) : true

      if (!result) {
        file.message(
          'Unknown rule: cannot ' + verb + " `'" + ruleId + "'`",
          node
        )
      }

      return result
    }

    /**
     * Get the latest state of a rule.
     * When without `ruleId`, gets global state.
     *
     * @param {string|undefined} ruleId
     * @returns {boolean}
     */
    function getState(ruleId) {
      const ranges = ruleId ? scope[ruleId] : globals

      if (ranges && ranges.length > 0) {
        return ranges[ranges.length - 1].state
      }

      if (!ruleId) {
        return !reset
      }

      return reset ? enable.includes(ruleId) : !disable.includes(ruleId)
    }

    /**
     * Handle a rule.
     *
     * @param {Point|undefined} point
     * @param {boolean} state
     * @param {string|undefined} [ruleId]
     * @returns {void}
     */
    function toggle(point, state, ruleId) {
      let markers = ruleId ? scope[ruleId] : globals

      if (!markers) {
        markers = []
        scope[String(ruleId)] = markers
      }

      const previousState = getState(ruleId)

      if (state !== previousState) {
        markers.push({state, point})
      }

      // Toggle all known rules.
      if (!ruleId) {
        for (ruleId in scope) {
          if (own.call(scope, ruleId)) {
            toggle(point, state, ruleId)
          }
        }
      }
    }

    /**
     * Check all `ranges` for `message`.
     *
     * @param {VFileMessage} message
     * @param {Mark[]|undefined} ranges
     * @param {string|undefined} [ruleId]
     * @returns {boolean}
     */
    function check(message, ranges, ruleId) {
      if (ranges && ranges.length > 0) {
        // Check the state at the message’s position.
        let index = ranges.length

        while (index--) {
          const range = ranges[index]

          if (
            message.line &&
            message.column &&
            range.point &&
            range.point.line &&
            range.point.column &&
            (range.point.line < message.line ||
              (range.point.line === message.line &&
                range.point.column <= message.column))
          ) {
            return range.state === true
          }
        }
      }

      // The first marker ocurred after the first message, so we check the
      // initial state.
      if (!ruleId) {
        return Boolean(initial || reset)
      }

      return reset ? enable.includes(ruleId) : !disable.includes(ruleId)
    }
  }
}

/**
 * Detect gaps in `tree`.
 *
 * @param {Node} tree
 * @param {VFile} file
 */
function detectGaps(tree, file) {
  /** @type {Node[]} */
  // @ts-expect-error: fine.
  const children = tree.children || []
  const lastNode = children[children.length - 1]
  /** @type {[number, number][]} */
  const gaps = []
  let offset = 0
  /** @type {boolean|undefined} */
  let gap

  // Find all gaps.
  visit(tree, one)

  // Get the end of the document.
  // This detects if the last node was the last node.
  // If not, there’s an extra gap between the last node and the end of the
  // document.
  if (
    lastNode &&
    lastNode.position &&
    lastNode.position.end &&
    offset === lastNode.position.end.offset &&
    file.toString().slice(offset).trim() !== ''
  ) {
    update()

    update(
      tree &&
        tree.position &&
        tree.position.end &&
        tree.position.end.offset &&
        tree.position.end.offset - 1
    )
  }

  return gaps

  /**
   * @param {Node} node
   */
  function one(node) {
    update(node.position && node.position.start && node.position.start.offset)

    if (!('children' in node)) {
      update(node.position && node.position.end && node.position.end.offset)
    }
  }

  /**
   * Detect a new position.
   *
   * @param {number|undefined} [latest]
   * @returns {void}
   */
  function update(latest) {
    if (latest === null || latest === undefined) {
      gap = true
    } else if (offset < latest) {
      if (gap) {
        gaps.push([offset, latest])
        gap = undefined
      }

      offset = latest
    }
  }
}
