'use strict'
var path = require('path')
var isWindows = require('./is-windows.js')

/*
Escape the name of an executable suitable for passing to the system shell.

Windows is easy, wrap in double quotes and you're done, as there's no
facility to create files with quotes in their names.

Unix-likes are a little more complicated, wrap in single quotes and escape
any single quotes in the filename.
*/

module.exports = escapify

function escapify (str) {
  if (isWindows) {
    return '"' + path.normalize(str) + '"'
  } else {
    if (/[^-_.~/\w]/.test(str)) {
      return "'" + str.replace(/'/g, "'\"'\"'") + "'"
    } else {
      return str
    }
  }
}
