/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module first-heading-level
 * @fileoverview
 *   Warn when the first heading has a level other than a specified value.
 *
 *   Options: `number`, default: `1`.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   # The default is to expect a level one heading
 *
 * @example
 *   {"name": "ok-html.md"}
 *
 *   <h1>An HTML heading is also seen by this rule.</h1>
 *
 * @example
 *   {"name": "ok-delayed.md"}
 *
 *   You can use markdown content before the heading.
 *
 *   <div>Or non-heading HTML</div>
 *
 *   <h1>So the first heading, be it HTML or markdown, is checked</h1>
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ## Bravo
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-1:9: First heading level should be `1`
 *
 * @example
 *   {"name": "not-ok-html.md", "label": "input"}
 *
 *   <h2>Charlie</h2>
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok-html.md", "label": "output"}
 *
 *   1:1-1:17: First heading level should be `1`
 *
 * @example
 *   {"name": "ok.md", "setting": 2}
 *
 *   ## Delta
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "ok-html.md", "setting": 2}
 *
 *   <h2>Echo</h2>
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "setting": 2, "label": "input"}
 *
 *   # Foxtrot
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok.md", "setting": 2, "label": "output"}
 *
 *   1:1-1:10: First heading level should be `2`
 *
 * @example
 *   {"name": "not-ok-html.md", "setting": 2, "label": "input"}
 *
 *   <h1>Golf</h1>
 *
 *   Paragraph.
 *
 * @example
 *   {"name": "not-ok-html.md", "setting": 2, "label": "output"}
 *
 *   1:1-1:14: First heading level should be `2`
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').HTML} HTML
 * @typedef {import('mdast').Heading['depth']} Depth
 * @typedef {Depth} Options
 */

import {lintRule} from 'unified-lint-rule'
import {visit, EXIT} from 'unist-util-visit'
import {generated} from 'unist-util-generated'

const re = /<h([1-6])/

const remarkLintFirstHeadingLevel = lintRule(
  'remark-lint:first-heading-level',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 1) => {
    visit(tree, (node) => {
      if (!generated(node)) {
        /** @type {Depth|undefined} */
        let rank

        if (node.type === 'heading') {
          rank = node.depth
        } else if (node.type === 'html') {
          rank = infer(node)
        }

        if (rank !== undefined) {
          if (rank !== option) {
            file.message('First heading level should be `' + option + '`', node)
          }

          return EXIT
        }
      }
    })
  }
)

export default remarkLintFirstHeadingLevel

/**
 * @param {HTML} node
 * @returns {Depth|undefined}
 */
function infer(node) {
  const results = node.value.match(re)
  // @ts-expect-error: can be castes fine.
  return results ? Number(results[1]) : undefined
}
