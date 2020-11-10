'use strict'
module.exports = function (dir, er) {
  if (!er) return
  var accessEr = new Error("EACCES, access '" + dir + "'", -13)
  accessEr.code = 'EACCES'
  accessEr.path = dir
  return accessEr
}
