/**
 * @typedef {import('../types.js').OmitHandle} OmitHandle
 */

import {isElement} from 'hast-util-is-element'
import {comment} from './util/comment.js'
import {siblingAfter} from './util/siblings.js'
import {whitespaceStart} from './util/whitespace-start.js'
import {omission} from './omission.js'

export const closing = omission({
  html,
  head: headOrColgroupOrCaption,
  body,
  p,
  li,
  dt,
  dd,
  rt: rubyElement,
  rp: rubyElement,
  optgroup,
  option,
  menuitem,
  colgroup: headOrColgroupOrCaption,
  caption: headOrColgroupOrCaption,
  thead,
  tbody,
  tfoot,
  tr,
  td: cells,
  th: cells
})

/**
 * Macro for `</head>`, `</colgroup>`, and `</caption>`.
 *
 * @type {OmitHandle}
 */
function headOrColgroupOrCaption(_, index, parent) {
  const next = siblingAfter(parent, index, true)
  return !next || (!comment(next) && !whitespaceStart(next))
}

/**
 * Whether to omit `</html>`.
 *
 * @type {OmitHandle}
 */
function html(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || !comment(next)
}

/**
 * Whether to omit `</body>`.
 *
 * @type {OmitHandle}
 */
function body(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || !comment(next)
}

/**
 * Whether to omit `</p>`.
 *
 * @type {OmitHandle}
 */
function p(_, index, parent) {
  const next = siblingAfter(parent, index)
  return next
    ? isElement(next, [
        'address',
        'article',
        'aside',
        'blockquote',
        'details',
        'div',
        'dl',
        'fieldset',
        'figcaption',
        'figure',
        'footer',
        'form',
        'h1',
        'h2',
        'h3',
        'h4',
        'h5',
        'h6',
        'header',
        'hgroup',
        'hr',
        'main',
        'menu',
        'nav',
        'ol',
        'p',
        'pre',
        'section',
        'table',
        'ul'
      ])
    : !parent ||
        // Confusing parent.
        !isElement(parent, [
          'a',
          'audio',
          'del',
          'ins',
          'map',
          'noscript',
          'video'
        ])
}

/**
 * Whether to omit `</li>`.
 *
 * @type {OmitHandle}
 */
function li(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, 'li')
}

/**
 * Whether to omit `</dt>`.
 *
 * @type {OmitHandle}
 */
function dt(_, index, parent) {
  const next = siblingAfter(parent, index)
  return next && isElement(next, ['dt', 'dd'])
}

/**
 * Whether to omit `</dd>`.
 *
 * @type {OmitHandle}
 */
function dd(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, ['dt', 'dd'])
}

/**
 * Whether to omit `</rt>` or `</rp>`.
 *
 * @type {OmitHandle}
 */
function rubyElement(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, ['rp', 'rt'])
}

/**
 * Whether to omit `</optgroup>`.
 *
 * @type {OmitHandle}
 */
function optgroup(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, 'optgroup')
}

/**
 * Whether to omit `</option>`.
 *
 * @type {OmitHandle}
 */
function option(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, ['option', 'optgroup'])
}

/**
 * Whether to omit `</menuitem>`.
 *
 * @type {OmitHandle}
 */
function menuitem(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, ['menuitem', 'hr', 'menu'])
}

/**
 * Whether to omit `</thead>`.
 *
 * @type {OmitHandle}
 */
function thead(_, index, parent) {
  const next = siblingAfter(parent, index)
  return next && isElement(next, ['tbody', 'tfoot'])
}

/**
 * Whether to omit `</tbody>`.
 *
 * @type {OmitHandle}
 */
function tbody(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, ['tbody', 'tfoot'])
}

/**
 * Whether to omit `</tfoot>`.
 *
 * @type {OmitHandle}
 */
function tfoot(_, index, parent) {
  return !siblingAfter(parent, index)
}

/**
 * Whether to omit `</tr>`.
 *
 * @type {OmitHandle}
 */
function tr(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, 'tr')
}

/**
 * Whether to omit `</td>` or `</th>`.
 *
 * @type {OmitHandle}
 */
function cells(_, index, parent) {
  const next = siblingAfter(parent, index)
  return !next || isElement(next, ['td', 'th'])
}
