const transientErrors = [
  'remote error: Internal Server Error',
  'The remote end hung up unexpectedly',
  'Connection timed out',
  'Operation timed out',
  'Failed to connect to .* Timed out',
  'Connection reset by peer',
  'SSL_ERROR_SYSCALL',
  'The requested URL returned error: 503'
].join('|')

const transientErrorRe = new RegExp(transientErrors)

const maxRetry = 3

module.exports = (error, number) =>
  transientErrorRe.test(error) && (number < maxRetry)
