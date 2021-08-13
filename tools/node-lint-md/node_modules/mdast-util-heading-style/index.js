/**
 * @typedef {import('mdast').Heading} Heading
 * @typedef {'atx'|'atx-closed'|'setext'} Style
 */

/**
 * @param {Heading} node
 * @param {Style} [relative]
 * @returns {Style|null}
 */
export function headingStyle(node, relative) {
  var last = node.children[node.children.length - 1]
  var depth = node.depth
  var pos = node && node.position && node.position.end
  var final = last && last.position && last.position.end

  if (!pos) {
    return null
  }

  // This can only occur for `'atx'` and `'atx-closed'` headings.
  // This might incorrectly match `'atx'` headings with lots of trailing white
  // space as an `'atx-closed'` heading.
  if (!last) {
    if (pos.column - 1 <= depth * 2) {
      return consolidate(depth, relative)
    }

    return 'atx-closed'
  }

  if (final.line + 1 === pos.line) {
    return 'setext'
  }

  if (final.column + depth < pos.column) {
    return 'atx-closed'
  }

  return consolidate(depth, relative)
}

/**
 * Get the probable style of an atx-heading, depending on preferred style.
 *
 * @param {number} depth
 * @param {Style} relative
 * @returns {Style|null}
 */
function consolidate(depth, relative) {
  return depth < 3
    ? 'atx'
    : relative === 'atx' || relative === 'setext'
    ? relative
    : null
}
