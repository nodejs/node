const { basename, dirname } = require('path')

const getName = (parent, base) =>
  parent.charAt(0) === '@' ? `${parent}/${base}` : base

module.exports = dir => dir ? getName(basename(dirname(dir)), basename(dir))
  : false
