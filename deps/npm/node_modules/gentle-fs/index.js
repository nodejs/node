'use strict'

const rm = require('./lib/rm.js')
const link = require('./lib/link.js')
const mkdir = require('./lib/mkdir.js')
const binLink = require('./lib/bin-link.js')

exports = module.exports = {
  rm: rm,
  link: link.link,
  linkIfExists: link.linkIfExists,
  mkdir: mkdir,
  binLink: binLink
}
