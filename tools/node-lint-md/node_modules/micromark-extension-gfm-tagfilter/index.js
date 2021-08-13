/**
 * @typedef {import('micromark-util-types').HtmlExtension} HtmlExtension
 * @typedef {import('micromark-util-types').Token} Token
 * @typedef {import('micromark-util-types').CompileContext} CompileContext
 */

/**
 * An opening or closing tag, followed by a case-insensitive specific tag name,
 * followed by HTML whitespace, a greater than, or a slash.
 */
const reFlow =
  /<(\/?)(iframe|noembed|noframes|plaintext|script|style|title|textarea|xmp)(?=[\t\n\f\r />])/gi

/**
 * As HTML (text) parses tags separately (and v. strictly), we don’t need to be
 * global.
 */
const reText = new RegExp('^' + reFlow.source, 'i')

/** @type {HtmlExtension} */
export const gfmTagfilterHtml = {
  exit: {
    htmlFlowData(token) {
      exitHtmlData.call(this, token, reFlow)
    },
    htmlTextData(token) {
      exitHtmlData.call(this, token, reText)
    }
  }
}

/**
 * @this {CompileContext}
 * @param {Token} token
 * @param {RegExp} filter
 */
function exitHtmlData(token, filter) {
  let value = this.sliceSerialize(token)

  if (this.options.allowDangerousHtml) {
    value = value.replace(filter, '&lt;$1$2')
  }

  this.raw(this.encode(value))
}
