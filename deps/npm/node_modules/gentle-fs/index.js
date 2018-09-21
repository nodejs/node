'use strict'

const rm = require('./lib/rm.js')
const link = require('./lib/link.js')

exports = module.exports = {
  rm: rm,
  link: link.link,
  linkIfExists: link.linkIfExists
}
