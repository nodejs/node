/**
 * @typedef {import('mdast').Root} Root
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('unified-message-control')} MessageControl
 * @typedef {Omit<import('unified-message-control').OptionsWithoutReset, 'marker'>|Omit<import('unified-message-control').OptionsWithReset, 'marker'>} Options
 */

import unifiedMessageControl from 'unified-message-control'
import {commentMarker} from 'mdast-comment-marker'

const test = [
  'html', // Comments are `html` nodes in mdast.
  'comment' // In MDX, comments have their own node.
]

/**
 * Plugin to enable, disable, and ignore messages.
 *
 * @type {import('unified').Plugin<[Options], Root>}
 * @returns {(node: Root, file: VFile) => void}
 */
export default function remarkMessageControl(options) {
  return unifiedMessageControl(
    Object.assign({marker: commentMarker, test}, options)
  )
}
