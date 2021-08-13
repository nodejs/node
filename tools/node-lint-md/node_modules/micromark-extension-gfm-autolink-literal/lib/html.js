/**
 * @typedef {import('micromark-util-types').HtmlExtension} HtmlExtension
 * @typedef {import('micromark-util-types').Handle} Handle
 * @typedef {import('micromark-util-types').CompileContext} CompileContext
 * @typedef {import('micromark-util-types').Token} Token
 */
import {sanitizeUri} from 'micromark-util-sanitize-uri'
/** @type {HtmlExtension} */

export const gfmAutolinkLiteralHtml = {
  exit: {
    literalAutolinkEmail,
    literalAutolinkHttp,
    literalAutolinkWww
  }
}
/** @type {Handle} */

function literalAutolinkWww(token) {
  anchorFromToken.call(this, token, 'http://')
}
/** @type {Handle} */

function literalAutolinkEmail(token) {
  anchorFromToken.call(this, token, 'mailto:')
}
/** @type {Handle} */

function literalAutolinkHttp(token) {
  anchorFromToken.call(this, token)
}
/**
 * @this CompileContext
 * @param {Token} token
 * @param {string} [protocol]
 * @returns {void}
 */

function anchorFromToken(token, protocol) {
  const url = this.sliceSerialize(token)
  this.tag('<a href="' + sanitizeUri((protocol || '') + url) + '">')
  this.raw(this.encode(url))
  this.tag('</a>')
}
