/**
 * @typedef {import('../../types.js').Comment} Comment
 */

import {convert} from 'unist-util-is'

/** @type {import('unist-util-is').AssertPredicate<Comment>} */
// @ts-ignore
export const comment = convert('comment')
