'use strict'

const util = require('util')

const contentPath = require('./path')
const { hasContent } = require('./read')
const rimraf = util.promisify(require('rimraf'))

module.exports = rm

async function rm (cache, integrity) {
  const content = await hasContent(cache, integrity)
  // ~pretty~ sure we can't end up with a content lacking sri, but be safe
  if (content && content.sri) {
    await rimraf(contentPath(cache, content.sri))
    return true
  } else {
    return false
  }
}
