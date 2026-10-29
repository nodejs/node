'use strict'

// const { parseHeaders } = require('../core/util')
const DecoratorHandler = require('../handler/decorator-handler')
const { InvalidArgumentError, ResponseError } = require('../core/errors')

const defaultMaxSize = 1024 * 1024

class ResponseErrorHandler extends DecoratorHandler {
  #statusCode
  #contentType
  #decoder
  #headers
  #body
  #bodySize
  #bodyTruncated
  #maxSize

  constructor ({ maxSize }, { handler }) {
    super(handler)
    this.#maxSize = maxSize
  }

  #checkContentType (contentType) {
    return (this.#contentType ?? '').indexOf(contentType) === 0
  }

  onRequestStart (controller, context) {
    this.#statusCode = 0
    this.#contentType = null
    this.#decoder = null
    this.#headers = null
    this.#body = ''
    this.#bodySize = 0
    this.#bodyTruncated = false

    return super.onRequestStart(controller, context)
  }

  onResponseStart (controller, statusCode, headers, statusMessage) {
    this.#statusCode = statusCode
    this.#headers = headers
    this.#contentType = headers['content-type']

    if (this.#statusCode < 400) {
      return super.onResponseStart(controller, statusCode, headers, statusMessage)
    }

    if (this.#checkContentType('application/json') || this.#checkContentType('text/plain')) {
      this.#decoder = new TextDecoder('utf-8')
    }
  }

  #createResponseError () {
    this.#body += this.#decoder?.decode(undefined, { stream: false }) ?? ''
    this.#decoder = null

    if (!this.#bodyTruncated && this.#checkContentType('application/json')) {
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
        bodyTruncated: this.#bodyTruncated,
        headers: this.#headers
      })
    } finally {
      Error.stackTraceLimit = stackTraceLimit
    }

    return err
  }

  onResponseData (controller, chunk) {
    if (this.#statusCode < 400) {
      return super.onResponseData(controller, chunk)
    }

    if (this.#decoder == null) {
      return
    }

    if (this.#maxSize === 0) {
      this.#body += this.#decoder.decode(chunk, { stream: true })
      return
    }

    const remaining = this.#maxSize - this.#bodySize

    if (chunk.length <= remaining) {
      this.#bodySize += chunk.length
      this.#body += this.#decoder.decode(chunk, { stream: true })
      return
    }

    if (remaining > 0) {
      this.#body += this.#decoder.decode(chunk.subarray(0, remaining), { stream: true })
      this.#bodySize = this.#maxSize
    }

    this.#bodyTruncated = true
    this.#body += this.#decoder.decode(undefined, { stream: false })
    this.#decoder = null
  }

  onResponseEnd (controller, trailers) {
    if (this.#statusCode >= 400) {
      super.onResponseError(controller, this.#createResponseError())
    } else {
      super.onResponseEnd(controller, trailers)
    }
  }

  onResponseError (controller, err) {
    super.onResponseError(controller, err)
  }
}

module.exports = (opts = {}) => {
  const { maxSize = defaultMaxSize } = opts

  if (!Number.isSafeInteger(maxSize) || maxSize < 0) {
    throw new InvalidArgumentError('maxSize must be a non-negative integer')
  }

  return (dispatch) => {
    return function Intercept (opts, handler) {
      return dispatch(opts, new ResponseErrorHandler({ maxSize }, { handler }))
    }
  }
}
