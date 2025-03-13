const {
  AUTH_HEADER,
  JSON_WEB_TOKEN,
  NPM_SECRET,
  DEEP_HEADER_AUTHORIZATION,
  DEEP_HEADER_SET_COOKIE,
  REWRITE_REQUEST,
  REWRITE_RESPONSE,
} = require('./matchers')

const {
  redactUrlMatcher,
  redactUrlPasswordMatcher,
  redactMatchers,
} = require('./utils')

const { serializeError } = require('./error')

const { deepMap } = require('./deep-map')

const _redact = redactMatchers(
  NPM_SECRET,
  AUTH_HEADER,
  JSON_WEB_TOKEN,
  DEEP_HEADER_AUTHORIZATION,
  DEEP_HEADER_SET_COOKIE,
  REWRITE_REQUEST,
  REWRITE_RESPONSE,
  redactUrlMatcher(
    redactUrlPasswordMatcher()
  )
)

const redact = (input) => deepMap(input, (value, path) => _redact(value, { path }))

/** takes an error returns new error keeping some custom properties */
function redactError (input) {
  const { message, ...data } = serializeError(input)
  const output = new Error(redact(message))
  return Object.assign(output, redact(data))
}

/** runs a function within try / catch and throws error wrapped in redactError */
function redactThrow (func) {
  if (typeof func !== 'function') {
    throw new Error('redactThrow expects a function')
  }
  return async (...args) => {
    try {
      return await func(...args)
    } catch (error) {
      throw redactError(error)
    }
  }
}

module.exports = { redact, redactError, redactThrow }
