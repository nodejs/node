const is = require('./is.js')
const { dirname } = require('path')
const check = (cwd, prev) => is({ cwd }).then(isGit =>
  isGit ? cwd
  : cwd === prev ? null
  : check(dirname(cwd), cwd))
module.exports = ({ cwd = process.cwd() } = {}) => check(cwd)
