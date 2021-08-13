/**
 * @typedef {import('micromark-util-types').Extension} Extension
 * @typedef {import('micromark-util-types').HtmlExtension} HtmlExtension
 * @typedef {import('micromark-extension-gfm-strikethrough').Options} Options
 */

import {
  combineExtensions,
  combineHtmlExtensions
} from 'micromark-util-combine-extensions'
import {
  gfmAutolinkLiteral,
  gfmAutolinkLiteralHtml
} from 'micromark-extension-gfm-autolink-literal'
import {
  gfmStrikethrough,
  gfmStrikethroughHtml
} from 'micromark-extension-gfm-strikethrough'
import {gfmTable, gfmTableHtml} from 'micromark-extension-gfm-table'
import {gfmTagfilterHtml} from 'micromark-extension-gfm-tagfilter'
import {
  gfmTaskListItem,
  gfmTaskListItemHtml
} from 'micromark-extension-gfm-task-list-item'

/**
 * Support GFM or markdown on github.com.
 *
 * @param {Options} [options]
 * @returns {Extension}
 */
export function gfm(options) {
  return combineExtensions([
    gfmAutolinkLiteral,
    gfmStrikethrough(options),
    gfmTable,
    gfmTaskListItem
  ])
}

/** @type {HtmlExtension} */
export const gfmHtml = combineHtmlExtensions([
  gfmAutolinkLiteralHtml,
  gfmStrikethroughHtml,
  gfmTableHtml,
  gfmTagfilterHtml,
  gfmTaskListItemHtml
])
