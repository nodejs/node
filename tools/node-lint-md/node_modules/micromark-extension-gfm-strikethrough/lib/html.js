/**
 * @typedef {import('micromark-util-types').HtmlExtension} HtmlExtension
 */

/** @type {HtmlExtension} */
export const gfmStrikethroughHtml = {
  enter: {
    strikethrough() {
      this.tag('<del>')
    }
  },
  exit: {
    strikethrough() {
      this.tag('</del>')
    }
  }
}
