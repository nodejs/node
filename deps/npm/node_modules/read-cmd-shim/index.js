'use strict'
var fs = require('graceful-fs')

function extractPath (path, cmdshimContents) {
  if (/[.]cmd$/.test(path)) {
    return extractPathFromCmd(cmdshimContents)
  } else if (/[.]ps1$/.test(path)) {
    return extractPathFromPowershell(cmdshimContents)
  } else {
    return extractPathFromCygwin(cmdshimContents)
  }
}

function extractPathFromPowershell (cmdshimContents) {
  var matches = cmdshimContents.match(/"[$]basedir[/]([^"]+?)"\s+[$]args/)
  return matches && matches[1]
}

function extractPathFromCmd (cmdshimContents) {
  var matches = cmdshimContents.match(/"%(?:~dp0|dp0%)\\([^"]+?)"\s+%[*]/)
  return matches && matches[1]
}

function extractPathFromCygwin (cmdshimContents) {
  var matches = cmdshimContents.match(/"[$]basedir[/]([^"]+?)"\s+"[$]@"/)
  return matches && matches[1]
}

function wrapError (thrown, newError) {
  newError.message = thrown.message
  newError.code = thrown.code
  return newError
}

function notaShim (path, er) {
  if (!er) {
    er = new Error()
    Error.captureStackTrace(er, notaShim)
  }
  er.code = 'ENOTASHIM'
  er.message = "Can't read shim path from '" + path + "', it doesn't appear to be a cmd-shim"
  return er
}

var readCmdShim = module.exports = function (path, cb) {
  var er = new Error()
  Error.captureStackTrace(er, readCmdShim)
  fs.readFile(path, function (readFileEr, contents) {
    if (readFileEr) return cb(wrapError(readFileEr, er))
    var destination = extractPath(path, contents.toString())
    if (destination) return cb(null, destination)
    return cb(notaShim(path, er))
  })
}

module.exports.sync = function (path) {
  var contents = fs.readFileSync(path)
  var destination = extractPath(path, contents.toString())
  if (!destination) throw notaShim(path)
  return destination
}
