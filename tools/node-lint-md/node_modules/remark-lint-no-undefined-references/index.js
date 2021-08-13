/**
 * @author Titus Wormer
 * @copyright 2016 Titus Wormer
 * @license MIT
 * @module no-undefined-references
 * @fileoverview
 *   Warn when references to undefined definitions are found.
 *
 *   Options: `Object`, optional.
 *
 *   The object can have an `allow` field, set to an array of strings that may
 *   appear between `[` and `]`, but that should not be treated as link
 *   identifiers.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   [foo][]
 *
 *   Just a [ bracket.
 *
 *   Typically, you’d want to use escapes (with a backslash: \\) to escape what
 *   could turn into a \[reference otherwise].
 *
 *   Just two braces can’t link: [].
 *
 *   [foo]: https://example.com
 *
 * @example
 *   {"name": "ok-allow.md", "setting": {"allow": ["...", "…"]}}
 *
 *   > Eliding a portion of a quoted passage […] is acceptable.
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   [bar]
 *
 *   [baz][]
 *
 *   [text][qux]
 *
 *   Spread [over
 *   lines][]
 *
 *   > in [a
 *   > block quote][]
 *
 *   [asd][a
 *
 *   Can include [*emphasis*].
 *
 *   Multiple pairs: [a][b][c].
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:6: Found reference to undefined definition
 *   3:1-3:8: Found reference to undefined definition
 *   5:1-5:12: Found reference to undefined definition
 *   7:8-8:9: Found reference to undefined definition
 *   10:6-11:17: Found reference to undefined definition
 *   13:1-13:6: Found reference to undefined definition
 *   15:13-15:25: Found reference to undefined definition
 *   17:17-17:23: Found reference to undefined definition
 *   17:23-17:26: Found reference to undefined definition
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Heading} Heading
 * @typedef {import('mdast').Paragraph} Paragraph
 *
 * @typedef Options
 * @property {string[]} [allow]
 *
 * @typedef {number[]} Range
 */

import {normalizeIdentifier} from 'micromark-util-normalize-identifier'
import {location} from 'vfile-location'
import {lintRule} from 'unified-lint-rule'
import {generated} from 'unist-util-generated'
import {pointStart, pointEnd} from 'unist-util-position'
import {visit, SKIP, EXIT} from 'unist-util-visit'

const remarkLintNoUndefinedReferences = lintRule(
  'remark-lint:no-undefined-references',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = {}) => {
    const contents = String(file)
    const loc = location(file)
    const lineEnding = /(\r?\n|\r)[\t ]*(>[\t ]*)*/g
    const allow = new Set(
      (option.allow || []).map((d) => normalizeIdentifier(d))
    )
    /** @type {Record<string, boolean>} */
    const map = Object.create(null)

    visit(tree, (node) => {
      if (
        (node.type === 'definition' || node.type === 'footnoteDefinition') &&
        !generated(node)
      ) {
        map[normalizeIdentifier(node.identifier)] = true
      }
    })

    visit(tree, (node) => {
      // CM specifiers that references only form when defined.
      // Still, they could be added by plugins, so let’s keep it.
      /* c8 ignore next 10 */
      if (
        (node.type === 'imageReference' ||
          node.type === 'linkReference' ||
          node.type === 'footnoteReference') &&
        !generated(node) &&
        !(normalizeIdentifier(node.identifier) in map) &&
        !allow.has(normalizeIdentifier(node.identifier))
      ) {
        file.message('Found reference to undefined definition', node)
      }

      if (node.type === 'paragraph' || node.type === 'heading') {
        findInPhrasing(node)
      }
    })

    /**
     * @param {Heading|Paragraph} node
     */
    function findInPhrasing(node) {
      /** @type {Range[]} */
      let ranges = []

      visit(node, (child) => {
        // Ignore the node itself.
        if (child === node) return

        // Can’t have links in links, so reset ranges.
        if (child.type === 'link' || child.type === 'linkReference') {
          ranges = []
          return SKIP
        }

        // Enter non-text.
        if (child.type !== 'text') return

        const start = pointStart(child).offset
        const end = pointEnd(child).offset

        // Bail if there’s no positional info.
        if (typeof start !== 'number' || typeof end !== 'number') {
          return EXIT
        }

        const source = contents.slice(start, end)
        /** @type {Array.<[number, string]>} */
        const lines = [[start, '']]
        let last = 0

        lineEnding.lastIndex = 0
        let match = lineEnding.exec(source)

        while (match) {
          const index = match.index
          lines[lines.length - 1][1] = source.slice(last, index)
          last = index + match[0].length
          lines.push([start + last, ''])
          match = lineEnding.exec(source)
        }

        lines[lines.length - 1][1] = source.slice(last)
        let lineIndex = -1

        while (++lineIndex < lines.length) {
          const line = lines[lineIndex][1]
          let index = 0

          while (index < line.length) {
            const code = line.charCodeAt(index)

            // Skip past escaped brackets.
            if (code === 92) {
              const next = line.charCodeAt(index + 1)
              index++

              if (next === 91 || next === 93) {
                index++
              }
            }
            // Opening bracket.
            else if (code === 91) {
              ranges.push([lines[lineIndex][0] + index])
              index++
            }
            // Close bracket.
            else if (code === 93) {
              // No opening.
              if (ranges.length === 0) {
                index++
              } else if (line.charCodeAt(index + 1) === 91) {
                index++

                // Collapsed or full.
                let range = ranges.pop()

                // Range should always exist.
                // eslint-disable-next-line max-depth
                if (range) {
                  range.push(lines[lineIndex][0] + index)

                  // This is the end of a reference already.
                  // eslint-disable-next-line max-depth
                  if (range.length === 4) {
                    handleRange(range)
                    range = []
                  }

                  range.push(lines[lineIndex][0] + index)
                  ranges.push(range)
                  index++
                }
              } else {
                index++

                // Shortcut or typical end of a reference.
                const range = ranges.pop()

                // Range should always exist.
                // eslint-disable-next-line max-depth
                if (range) {
                  range.push(lines[lineIndex][0] + index)
                  handleRange(range)
                }
              }
            }
            // Anything else.
            else {
              index++
            }
          }
        }
      })

      let index = -1

      while (++index < ranges.length) {
        handleRange(ranges[index])
      }

      return SKIP

      /**
       * @param {Range} range
       */
      function handleRange(range) {
        if (range.length === 1) return
        if (range.length === 3) range.length = 2

        // No need to warn for just `[]`.
        if (range.length === 2 && range[0] + 2 === range[1]) return

        const offset = range.length === 4 && range[2] + 2 !== range[3] ? 2 : 0
        const id = contents
          .slice(range[0 + offset] + 1, range[1 + offset] - 1)
          .replace(lineEnding, ' ')
        const pos = {
          start: loc.toPoint(range[0]),
          end: loc.toPoint(range[range.length - 1])
        }

        if (
          !generated({position: pos}) &&
          !(normalizeIdentifier(id) in map) &&
          !allow.has(normalizeIdentifier(id))
        ) {
          file.message('Found reference to undefined definition', pos)
        }
      }
    }
  }
)

export default remarkLintNoUndefinedReferences
