/**
 * @typedef {import('mdast').Root|import('mdast').Content} Node
 * @typedef {import('mdast-util-to-markdown').Options} Options
 */

import {toMarkdown} from 'mdast-util-to-markdown'

/** @type {import('unified').Plugin<[Options]|void[], Node, string>} */
export default function remarkStringify(options) {
  /** @type {import('unified').CompilerFunction<Node, string>} */
  const compiler = (tree) => {
    // Assume options.
    const settings = /** @type {Options} */ (this.data('settings'))

    return toMarkdown(
      tree,
      Object.assign({}, settings, options, {
        // Note: this option is not in the readme.
        // The goal is for it to be set by plugins on `data` instead of being
        // passed by users.
        extensions: this.data('toMarkdownExtensions') || []
      })
    )
  }

  Object.assign(this, {Compiler: compiler})
}
