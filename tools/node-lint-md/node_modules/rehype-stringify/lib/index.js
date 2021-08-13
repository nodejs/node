/**
 * @typedef {import('hast').Root} Root
 * @typedef {Root|Root['children'][number]} Node
 * @typedef {import('hast-util-to-html').Options} Options
 */

import {toHtml} from 'hast-util-to-html'

/** @type {import('unified').Plugin<[Options]|void[], Node, string>} */
export default function rehypeStringify(config) {
  const processorSettings = /** @type {Options} */ (this.data('settings'))
  const settings = Object.assign({}, config, processorSettings)

  Object.assign(this, {Compiler: compiler})

  /**
   * @type {import('unified').CompilerFunction<Node, string>}
   */
  function compiler(tree) {
    return toHtml(tree, settings)
  }
}
