const {
  GitConnectionError,
  GitPathspecError,
  GitUnknownError
} = require('./errors.js')

const connectionErrorRe = new RegExp([
  'remote error: Internal Server Error',
  'The remote end hung up unexpectedly',
  'Connection timed out',
  'Operation timed out',
  'Failed to connect to .* Timed out',
  'Connection reset by peer',
  'SSL_ERROR_SYSCALL',
  'The requested URL returned error: 503'
].join('|'))

const missingPathspecRe = /pathspec .* did not match any file\(s\) known to git/

function makeError (er) {
  const message = er.stderr
  let gitEr
  if (connectionErrorRe.test(message)) {
    gitEr = new GitConnectionError(message)
  } else if (missingPathspecRe.test(message)) {
    gitEr = new GitPathspecError(message)
  } else {
    gitEr = new GitUnknownError(message)
  }
  return Object.assign(gitEr, er)
}

module.exports = makeError
