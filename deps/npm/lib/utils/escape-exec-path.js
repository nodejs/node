const { normalize } = require('path')
const isWindows = require('./is-windows.js')

/*
Escape the name of an executable suitable for passing to the system shell.

Windows is easy, wrap in double quotes and you're done, as there's no
facility to create files with quotes in their names.

Unix-likes are a little more complicated, wrap in single quotes and escape
any single quotes in the filename.  The '"'"' construction ends the quoted
block, creates a new " quoted string with ' in it.  So, `foo'bar` becomes
`'foo'"'"'bar'`, which is the bash way of saying `'foo' + "'" + 'bar'`.
*/

const winQuote = str => !/ /.test(str) ? str : '"' + str + '"'
const winEsc = str => normalize(str).split(/\\/).map(winQuote).join('\\')

module.exports = str => isWindows ? winEsc(str)
  : /[^-_.~/\w]/.test(str) ? "'" + str.replace(/'/g, '\'"\'"\'') + "'"
  : str
