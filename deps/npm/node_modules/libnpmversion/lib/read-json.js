// can't use read-package-json-fast, because we want to ensure
// that we make as few changes as possible, even for safety issues.
const { promisify } = require('util')
const readFile = promisify(require('fs').readFile)
const parse = require('json-parse-even-better-errors')

module.exports = async path => parse(await readFile(path))
