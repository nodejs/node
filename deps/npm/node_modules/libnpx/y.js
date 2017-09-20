'use strict'

const path = require('path')
const yargs = require('yargs')
const y18n = require('y18n')({
  directory: path.join(__dirname, 'locales'),
  locale: yargs.locale(),
  updateFiles: process.env.NPX_UPDATE_LOCALE_FILES === 'true'
})

module.exports = yTag
function yTag (parts) {
  let str = ''
  parts.forEach((part, i) => {
    str += part
    if (arguments.length > i + 1) {
      str += '%s'
    }
  })
  return y18n.__.apply(null, [str].concat([].slice.call(arguments, 1)))
}
