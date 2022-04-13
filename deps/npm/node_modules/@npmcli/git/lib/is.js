// not an airtight indicator, but a good gut-check to even bother trying
const { promisify } = require('util')
const fs = require('fs')
const stat = promisify(fs.stat)
module.exports = ({ cwd = process.cwd() } = {}) =>
  stat(cwd + '/.git').then(() => true, () => false)
