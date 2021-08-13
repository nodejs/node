/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('mdast-util-from-markdown').Options} Options
 */

import {fromMarkdown} from 'mdast-util-from-markdown'

/** @type {import('unified').Plugin<[Options?] | void[], string, Root>} */
export default function remarkParse(options) {
  /** @type {import('unified').ParserFunction<Root>} */
  const parser = (doc) => {
    // Assume options.
    const settings = /** @type {Options} */ (this.data('settings'))

    return fromMarkdown(
      doc,
      Object.assign({}, settings, options, {
        // Note: these options are not in the readme.
        // The goal is for them to be set by plugins on `data` instead of being
        // passed by users.
        extensions: this.data('micromarkExtensions') || [],
        mdastExtensions: this.data('fromMarkdownExtensions') || []
      })
    )
  }

  Object.assign(this, {Parser: parser})
}
