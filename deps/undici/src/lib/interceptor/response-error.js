'use strict'

const { parseHeaders } = require('../core/util')
const DecoratorHandler = require('../handler/decorator-handler')
const { ResponseError } = require('../core/errors')

class ResponseErrorHandler extends DecoratorHandler {
  #handler
  #statusCode
  #contentType
  #decoder
  #headers
  #body

  constructor (opts, { handler }) {
    super(handler)
    this.#handler = handler
  }

  onConnect (abort) {
    this.#statusCode = 0
    this.#contentType = null
    this.#decoder = null
    this.#headers = null
    this.#body = ''

    return this.#handler.onConnect(abort)
  }

  #checkContentType (contentType) {
    return this.#contentType.indexOf(contentType) === 0
  }

  onHeaders (statusCode, rawHeaders, resume, statusMessage, headers = parseHeaders(rawHeaders)) {
    this.#statusCode = statusCode
    this.#headers = headers
    this.#contentType = headers['content-type']

    if (this.#statusCode < 400) {
      return this.#handler.onHeaders(statusCode, rawHeaders, resume, statusMessage, headers)
    }

    if (this.#checkContentType('application/json') || this.#checkContentType('text/plain')) {
      this.#decoder = new TextDecoder('utf-8')
    }
  }

  onData (chunk) {
    if (this.#statusCode < 400) {
      return this.#handler.onData(chunk)
    }

    this.#body += this.#decoder?.decode(chunk, { stream: true }) ?? ''
  }

  onComplete (rawTrailers) {
    if (this.#statusCode >= 400) {
      this.#body += this.#decoder?.decode(undefined, { stream: false }) ?? ''

      if (this.#checkContentType('application/json')) {
        try {
          this.#body = JSON.parse(this.#body)
        } catch {
          // Do nothing...
        }
      }

      let err
      const stackTraceLimit = Error.stackTraceLimit
      Error.stackTraceLimit = 0
      try {
        err = new ResponseError('Response Error', this.#statusCode, {
          body: this.#body,
          headers: this.#headers
        })
      } finally {
        Error.stackTraceLimit = stackTraceLimit
      }

      this.#handler.onError(err)
    } else {
      this.#handler.onComplete(rawTrailers)
    }
  }

  onError (err) {
    this.#handler.onError(err)
  }
}

module.exports = () => {
  return (dispatch) => {
    return function Intercept (opts, handler) {
      return dispatch(opts, new ResponseErrorHandler(opts, { handler }))
    }
  }
}
