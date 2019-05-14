'use strict'

/**
 * fetch-error.js
 *
 * FetchError interface for operational errors
 */

/**
 * Create FetchError instance
 *
 * @param   String      message      Error message for human
 * @param   String      type         Error type for machine
 * @param   String      systemError  For Node.js system error
 * @return  FetchError
 */
module.exports = FetchError
function FetchError (message, type, systemError) {
  Error.call(this, message)

  this.message = message
  this.type = type

  // when err.type is `system`, err.code contains system error code
  if (systemError) {
    this.code = this.errno = systemError.code
  }

  // hide custom error implementation details from end-users
  Error.captureStackTrace(this, this.constructor)
}

FetchError.prototype = Object.create(Error.prototype)
FetchError.prototype.constructor = FetchError
FetchError.prototype.name = 'FetchError'
