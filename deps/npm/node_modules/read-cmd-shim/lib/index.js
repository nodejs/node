const fs = require('fs')
const { promisify } = require('util')
const { readFileSync } = fs
const readFile = promisify(fs.readFile)

const extractPath = (path, cmdshimContents) => {
  if (/[.]cmd$/.test(path)) {
    return extractPathFromCmd(cmdshimContents)
  } else if (/[.]ps1$/.test(path)) {
    return extractPathFromPowershell(cmdshimContents)
  } else {
    return extractPathFromCygwin(cmdshimContents)
  }
}

const extractPathFromPowershell = cmdshimContents => {
  const matches = cmdshimContents.match(/"[$]basedir[/]([^"]+?)"\s+[$]args/)
  return matches && matches[1]
}

const extractPathFromCmd = cmdshimContents => {
  const matches = cmdshimContents.match(/"%(?:~dp0|dp0%)\\([^"]+?)"\s+%[*]/)
  return matches && matches[1]
}

const extractPathFromCygwin = cmdshimContents => {
  const matches = cmdshimContents.match(/"[$]basedir[/]([^"]+?)"\s+"[$]@"/)
  return matches && matches[1]
}

const wrapError = (thrown, newError) => {
  newError.message = thrown.message
  newError.code = thrown.code
  newError.path = thrown.path
  return newError
}

const notaShim = (path, er) => {
  if (!er) {
    er = new Error()
    Error.captureStackTrace(er, notaShim)
  }
  er.code = 'ENOTASHIM'
  er.message = `Can't read shim path from '${path}', ` +
    `it doesn't appear to be a cmd-shim`
  return er
}

const readCmdShim = path => {
  // create a new error to capture the stack trace from this point,
  // instead of getting some opaque stack into node's internals
  const er = new Error()
  Error.captureStackTrace(er, readCmdShim)
  return readFile(path).then(contents => {
    const destination = extractPath(path, contents.toString())
    if (destination) {
      return destination
    }
    return Promise.reject(notaShim(path, er))
  }, readFileEr => Promise.reject(wrapError(readFileEr, er)))
}

const readCmdShimSync = path => {
  const contents = readFileSync(path)
  const destination = extractPath(path, contents.toString())
  if (!destination) {
    throw notaShim(path)
  }
  return destination
}

readCmdShim.sync = readCmdShimSync
module.exports = readCmdShim
