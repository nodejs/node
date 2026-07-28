const { log, output } = require('proc-log')
const { redactLog: replaceInfo } = require('@npmcli/redact')

// Print an error or just nothing if the audit report has an error.
// This is called by the audit command, and by the reify-output util prints a JSON version of the error if it's --json.
// Returns 'true' if there was an error, false otherwise.

const auditError = (npm, report) => {
  if (!report?.error) {
    return false
  }

  if (npm.command !== 'audit') {
    return true
  }

  const { error } = report

  // ok, we care about it, then
  log.warn('audit', error.message)
  const { body: errBody } = error
  const body = Buffer.isBuffer(errBody) ? errBody.toString() : errBody
  if (npm.flatOptions.json) {
    output.buffer({
      message: error.message,
      method: error.method,
      uri: replaceInfo(error.uri),
      headers: error.headers,
      statusCode: error.statusCode,
      body,
    })
  } else {
    output.standard(body)
  }

  // XXX we should throw a real error here
  throw 'audit endpoint returned an error'
}

module.exports = auditError
