/**
 * @typedef {import('micromark-util-types').HtmlExtension} HtmlExtension
 */

/**
 * @typedef {import('./syntax.js').Align} Align
 */

const alignment = {
  null: '',
  left: ' align="left"',
  right: ' align="right"',
  center: ' align="center"'
}

/** @type {HtmlExtension} */
export const gfmTableHtml = {
  enter: {
    table(token) {
      this.lineEndingIfNeeded()
      this.tag('<table>')
      // @ts-expect-error Custom.
      this.setData('tableAlign', token._align)
    },
    tableBody() {
      // Clear slurping line ending from the delimiter row.
      this.setData('slurpOneLineEnding')
      this.tag('<tbody>')
    },
    tableData() {
      /** @type {string|undefined} */
      const align =
        // @ts-expect-error Custom.
        alignment[this.getData('tableAlign')[this.getData('tableColumn')]]

      if (align === undefined) {
        // Capture results to ignore them.
        this.buffer()
      } else {
        this.lineEndingIfNeeded()
        this.tag('<td' + align + '>')
      }
    },
    tableHead() {
      this.lineEndingIfNeeded()
      this.tag('<thead>')
    },
    tableHeader() {
      this.lineEndingIfNeeded()
      this.tag(
        '<th' +
          // @ts-expect-error Custom.
          alignment[this.getData('tableAlign')[this.getData('tableColumn')]] +
          '>'
      )
    },
    tableRow() {
      this.setData('tableColumn', 0)
      this.lineEndingIfNeeded()
      this.tag('<tr>')
    }
  },
  exit: {
    // Overwrite the default code text data handler to unescape escaped pipes when
    // they are in tables.
    codeTextData(token) {
      let value = this.sliceSerialize(token)

      if (this.getData('tableAlign')) {
        value = value.replace(/\\([\\|])/g, replace)
      }

      this.raw(this.encode(value))
    },
    table() {
      this.setData('tableAlign')
      // If there was no table body, make sure the slurping from the delimiter row
      // is cleared.
      this.setData('slurpAllLineEndings')
      this.lineEndingIfNeeded()
      this.tag('</table>')
    },
    tableBody() {
      this.lineEndingIfNeeded()
      this.tag('</tbody>')
    },
    tableData() {
      /** @type {number} */
      // @ts-expect-error Custom.
      const column = this.getData('tableColumn')

      // @ts-expect-error Custom.
      if (column in this.getData('tableAlign')) {
        this.tag('</td>')
        this.setData('tableColumn', column + 1)
      } else {
        // Stop capturing.
        this.resume()
      }
    },
    tableHead() {
      this.lineEndingIfNeeded()
      this.tag('</thead>')
      this.setData('slurpOneLineEnding', true)
      // Slurp the line ending from the delimiter row.
    },
    tableHeader() {
      this.tag('</th>')
      // @ts-expect-error Custom.
      this.setData('tableColumn', this.getData('tableColumn') + 1)
    },
    tableRow() {
      /** @type {Align[]} */
      // @ts-expect-error Custom.
      const align = this.getData('tableAlign')
      /** @type {number} */
      // @ts-expect-error Custom.
      let column = this.getData('tableColumn')

      while (column < align.length) {
        this.lineEndingIfNeeded()
        // @ts-expect-error `null` is fine as an index.
        this.tag('<td' + alignment[align[column]] + '></td>')
        column++
      }

      this.setData('tableColumn', column)
      this.lineEndingIfNeeded()
      this.tag('</tr>')
    }
  }
}

/**
 * @param {string} $0
 * @param {string} $1
 * @returns {string}
 */
function replace($0, $1) {
  // Pipes work, backslashes don’t (but can’t escape pipes).
  return $1 === '|' ? $1 : $0
}
