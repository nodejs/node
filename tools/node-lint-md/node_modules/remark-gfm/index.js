/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('micromark-extension-gfm').Options & import('mdast-util-gfm').Options} Options
 */

import {gfm} from 'micromark-extension-gfm'
import {gfmFromMarkdown, gfmToMarkdown} from 'mdast-util-gfm'

/**
 * Plugin to support GitHub Flavored Markdown (GFM).
 *
 * @type {import('unified').Plugin<[Options?]|void[], Root>}
 */
export default function remarkGfm(options = {}) {
  const data = this.data()

  add('micromarkExtensions', gfm(options))
  add('fromMarkdownExtensions', gfmFromMarkdown)
  add('toMarkdownExtensions', gfmToMarkdown(options))

  /**
   * @param {string} field
   * @param {unknown} value
   */
  function add(field, value) {
    const list = /** @type {unknown[]} */ (
      // Other extensions
      /* c8 ignore next 2 */
      data[field] ? data[field] : (data[field] = [])
    )

    list.push(value)
  }
}
