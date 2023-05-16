const { relative } = require('path')
const relpath = (from, to) => relative(from, to).replace(/\\/g, '/')
module.exports = relpath
