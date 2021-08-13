/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shell-dollars
 * @fileoverview
 *   Warn when shell code is prefixed by `$` (dollar sign) characters.
 *
 *   Ignores indented code blocks and fenced code blocks without language flag.
 *
 * @example
 *   {"name": "ok.md"}
 *
 *   ```bash
 *   echo a
 *   ```
 *
 *   ```sh
 *   echo a
 *   echo a > file
 *   ```
 *
 *   ```zsh
 *   $ echo a
 *   a
 *   $ echo a > file
 *   ```
 *
 *   Some empty code:
 *
 *   ```command
 *   ```
 *
 *   It’s fine to use dollars in non-shell code.
 *
 *   ```js
 *   $('div').remove()
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "label": "input"}
 *
 *   ```sh
 *   $ echo a
 *   ```
 *
 *   ```bash
 *   $ echo a
 *   $ echo a > file
 *   ```
 *
 * @example
 *   {"name": "not-ok.md", "label": "output"}
 *
 *   1:1-3:4: Do not use dollar signs before shell commands
 *   5:1-8:4: Do not use dollar signs before shell commands
 */

/**
 * @typedef {import('mdast').Root} Root
 */

import {lintRule} from 'unified-lint-rule'
import {visit} from 'unist-util-visit'
import {generated} from 'unist-util-generated'

// List of shell script file extensions (also used as code flags for syntax
// highlighting on GitHub):
// See: <https://github.com/github/linguist/blob/40992ba/lib/linguist/languages.yml#L4984>
const flags = new Set([
  'sh',
  'bash',
  'bats',
  'cgi',
  'command',
  'fcgi',
  'ksh',
  'tmux',
  'tool',
  'zsh'
])

const remarkLintNoShellDollars = lintRule(
  'remark-lint:no-shell-dollars',
  /** @type {import('unified-lint-rule').Rule<Root, void>} */
  (tree, file) => {
    visit(tree, 'code', (node) => {
      // Check both known shell code and unknown code.
      if (!generated(node) && node.lang && flags.has(node.lang)) {
        const lines = node.value
          .split('\n')
          .filter((line) => line.trim().length > 0)
        let index = -1

        if (lines.length === 0) {
          return
        }

        while (++index < lines.length) {
          const line = lines[index]

          if (line.trim() && !/^\s*\$\s*/.test(line)) {
            return
          }
        }

        file.message('Do not use dollar signs before shell commands', node)
      }
    })
  }
)

export default remarkLintNoShellDollars
