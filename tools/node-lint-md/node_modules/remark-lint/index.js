/**
 * @typedef {import('mdast').Root} Root
 */

import remarkMessageControl from 'remark-message-control'

/**
 * The core plugin for `remark-lint`.
 * This adds support for ignoring stuff from messages (`<!--lint ignore-->`).
 * All rules are in their own packages and presets.
 *
 * @type {import('unified').Plugin<void[], Root>}
 */
export default function remarkLint() {
  this.use(lintMessageControl)
}

/** @type {import('unified').Plugin<void[], Root>} */
function lintMessageControl() {
  return remarkMessageControl({name: 'lint', source: 'remark-lint'})
}
