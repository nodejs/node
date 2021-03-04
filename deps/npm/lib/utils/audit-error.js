// print an error or just nothing if the audit report has an error
// this is called by the audit command, and by the reify-output util
// prints a JSON version of the error if it's --json
// returns 'true' if there was an error, false otherwise

const output = require('./output.js')
const auditError = (npm, report) => {
  if (!report || !report.error)
    return false

  if (npm.command !== 'audit')
    return true

  const { error } = report

  // ok, we care about it, then
  npm.log.warn('audit', error.message)
  const { body: errBody } = error
  const body = Buffer.isBuffer(errBody) ? errBody.toString() : errBody
  if (npm.flatOptions.json) {
    output(JSON.stringify({
      message: error.message,
      method: error.method,
      uri: error.uri,
      headers: error.headers,
      statusCode: error.statusCode,
      body,
    }, null, 2))
  } else
    output(body)

  throw 'audit endpoint returned an error'
}

module.exports = auditError
