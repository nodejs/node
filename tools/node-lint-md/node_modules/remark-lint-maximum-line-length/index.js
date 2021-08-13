/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module maximum-line-length
 * @fileoverview
 *   Warn when lines are too long.
 *
 *   Options: `number`, default: `80`.
 *
 *   Ignores nodes that cannot be wrapped, such as headings, tables, code,
 *   definitions, HTML, and JSX.
 *
 *   Ignores images, links, and inline code if they start before the wrap, end
 *   after the wrap, and there’s no whitespace after them.
 *
 * @example
 *   {"name": "ok.md", "positionless": true, "gfm": true}
 *
 *   This line is simply not toooooooooooooooooooooooooooooooooooooooooooo
 *   long.
 *
 *   This is also fine: <http://this-long-url-with-a-long-domain.co.uk/a-long-path?query=variables>
 *
 *   <http://this-link-is-fine.com>
 *
 *   `alphaBravoCharlieDeltaEchoFoxtrotGolfHotelIndiaJuliettKiloLimaMikeNovemberOscarPapaQuebec.romeo()`
 *
 *   [foo](http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables)
 *
 *   <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>
 *
 *   ![foo](http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables)
 *
 *   | An | exception | is | line | length | in | long | tables | because | those | can’t | just |
 *   | -- | --------- | -- | ---- | ------ | -- | ---- | ------ | ------- | ----- | ----- | ---- |
 *   | be | helped    |    |      |        |    |      |        |         |       |       | .    |
 *
 *   <a><b><i><p><q><s><u>alpha bravo charlie delta echo foxtrot golf</u></s></q></p></i></b></a>
 *
 *   The following is also fine, because there is no whitespace.
 *
 *   <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>.
 *
 *   In addition, definitions are also fine:
 *
 *   [foo]: <http://this-long-url-with-a-long-domain-is-ok.co.uk/a-long-path?query=variables>
 *
 * @example
 *   {"name": "not-ok.md", "setting": 80, "label": "input", "positionless": true}
 *
 *   This line is simply not tooooooooooooooooooooooooooooooooooooooooooooooooooooooo
 *   long.
 *
 *   Just like thiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiis one.
 *
 *   And this one is also very wrong: because the link starts aaaaaaafter the column: <http://line.com>
 *
 *   <http://this-long-url-with-a-long-domain-is-not-ok.co.uk/a-long-path?query=variables> and such.
 *
 *   And this one is also very wrong: because the code starts aaaaaaafter the column: `alpha.bravo()`
 *
 *   `alphaBravoCharlieDeltaEchoFoxtrotGolfHotelIndiaJuliettKiloLimaMikeNovemberOscar.papa()` and such.
 *
 * @example
 *   {"name": "not-ok.md", "setting": 80, "label": "output", "positionless": true}
 *
 *   4:86: Line must be at most 80 characters
 *   6:99: Line must be at most 80 characters
 *   8:96: Line must be at most 80 characters
 *   10:97: Line must be at most 80 characters
 *   12:99: Line must be at most 80 characters
 *
 * @example
 *   {"name": "ok-mixed-line-endings.md", "setting": 10, "positionless": true}
 *
 *   0123456789␍␊
 *   0123456789␊
 *   01234␍␊
 *   01234␊
 *
 * @example
 *   {"name": "not-ok-mixed-line-endings.md", "setting": 10, "label": "input", "positionless": true}
 *
 *   012345678901␍␊
 *   012345678901␊
 *   01234567890␍␊
 *   01234567890␊
 *
 * @example
 *   {"name": "not-ok-mixed-line-endings.md", "setting": 10, "label": "output", "positionless": true}
 *
 *   1:13: Line must be at most 10 characters
 *   2:13: Line must be at most 10 characters
 *   3:12: Line must be at most 10 characters
 *   4:12: Line must be at most 10 characters
 */

/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast').Parent} Parent
 * @typedef {number} Options
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {pointStart, pointEnd} from 'unist-util-position'
import {generated} from 'unist-util-generated'

const remarkLintMaximumLineLength = lintRule(
  'remark-lint:maximum-line-length',
  /** @type {import('unified-lint-rule').Rule<Root, Options>} */
  (tree, file, option = 80) => {
    const value = String(file)
    const lines = value.split(/\r?\n/)

    visit(tree, (node) => {
      if (
        (node.type === 'heading' ||
          node.type === 'table' ||
          node.type === 'code' ||
          node.type === 'definition' ||
          node.type === 'html' ||
          // @ts-expect-error: JSX is from MDX: <https://github.com/mdx-js/specification>.
          node.type === 'jsx' ||
          node.type === 'yaml' ||
          // @ts-expect-error: TOML is from frontmatter.
          node.type === 'toml') &&
        !generated(node)
      ) {
        allowList(pointStart(node).line - 1, pointEnd(node).line)
      }
    })

    // Finally, allow some inline spans, but only if they occur at or after
    // the wrap.
    // However, when they do, and there’s whitespace after it, they are not
    // allowed.
    visit(tree, (node, pos, parent_) => {
      const parent = /** @type {Parent} */ (parent_)

      if (
        (node.type === 'link' ||
          node.type === 'image' ||
          node.type === 'inlineCode') &&
        !generated(node) &&
        parent &&
        typeof pos === 'number'
      ) {
        const initial = pointStart(node)
        const final = pointEnd(node)

        // Not allowing when starting after the border, or ending before it.
        if (initial.column > option || final.column < option) {
          return
        }

        const next = parent.children[pos + 1]

        // Not allowing when there’s whitespace after the link.
        if (
          next &&
          pointStart(next).line === initial.line &&
          (!('value' in next) || /^(.+?[ \t].+?)/.test(next.value))
        ) {
          return
        }

        allowList(initial.line - 1, final.line)
      }
    })

    // Iterate over every line, and warn for violating lines.
    let index = -1

    while (++index < lines.length) {
      const lineLength = lines[index].length

      if (lineLength > option) {
        file.message('Line must be at most ' + option + ' characters', {
          line: index + 1,
          column: lineLength + 1
        })
      }
    }

    /**
     * Allowlist from `initial` to `final`, zero-based.
     *
     * @param {number} initial
     * @param {number} final
     */
    function allowList(initial, final) {
      while (initial < final) {
        lines[initial++] = ''
      }
    }
  }
)

export default remarkLintMaximumLineLength
