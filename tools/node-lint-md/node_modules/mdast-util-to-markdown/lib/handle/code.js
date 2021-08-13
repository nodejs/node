/**
 * @typedef {import('mdast').Code} Code
 * @typedef {import('../types.js').Handle} Handle
 * @typedef {import('../types.js').Exit} Exit
 * @typedef {import('../util/indent-lines.js').Map} Map
 */

import {longestStreak} from 'longest-streak'
import {formatCodeAsIndented} from '../util/format-code-as-indented.js'
import {checkFence} from '../util/check-fence.js'
import {indentLines} from '../util/indent-lines.js'
import {safe} from '../util/safe.js'

/**
 * @type {Handle}
 * @param {Code} node
 */
export function code(node, _, context) {
  const marker = checkFence(context)
  const raw = node.value || ''
  const suffix = marker === '`' ? 'GraveAccent' : 'Tilde'
  /** @type {string} */
  let value
  /** @type {Exit} */
  let exit

  if (formatCodeAsIndented(node, context)) {
    exit = context.enter('codeIndented')
    value = indentLines(raw, map)
  } else {
    const sequence = marker.repeat(Math.max(longestStreak(raw, marker) + 1, 3))
    /** @type {Exit} */
    let subexit
    exit = context.enter('codeFenced')
    value = sequence

    if (node.lang) {
      subexit = context.enter('codeFencedLang' + suffix)
      value += safe(context, node.lang, {
        before: '`',
        after: ' ',
        encode: ['`']
      })
      subexit()
    }

    if (node.lang && node.meta) {
      subexit = context.enter('codeFencedMeta' + suffix)
      value +=
        ' ' +
        safe(context, node.meta, {
          before: ' ',
          after: '\n',
          encode: ['`']
        })
      subexit()
    }

    value += '\n'

    if (raw) {
      value += raw + '\n'
    }

    value += sequence
  }

  exit()
  return value
}

/** @type {Map} */
function map(line, _, blank) {
  return (blank ? '' : '    ') + line
}
