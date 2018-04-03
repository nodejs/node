'use strict'

/**
 * response.js
 *
 * Response class provides content decoding
 */

const STATUS_CODES = require('http').STATUS_CODES
const Headers = require('./headers.js')
const Body = require('./body.js')
const clone = Body.clone

/**
 * Response class
 *
 * @param   Stream  body  Readable stream
 * @param   Object  opts  Response options
 * @return  Void
 */
class Response {
  constructor (body, opts) {
    if (!opts) opts = {}
    Body.call(this, body, opts)

    this.url = opts.url
    this.status = opts.status || 200
    this.statusText = opts.statusText || STATUS_CODES[this.status]

    this.headers = new Headers(opts.headers)

    Object.defineProperty(this, Symbol.toStringTag, {
      value: 'Response',
      writable: false,
      enumerable: false,
      configurable: true
    })
  }

  /**
   * Convenience property representing if the request ended normally
   */
  get ok () {
    return this.status >= 200 && this.status < 300
  }

  /**
   * Clone this response
   *
   * @return  Response
   */
  clone () {
    return new Response(clone(this), {
      url: this.url,
      status: this.status,
      statusText: this.statusText,
      headers: this.headers,
      ok: this.ok
    })
  }
}

Body.mixIn(Response.prototype)

Object.defineProperty(Response.prototype, Symbol.toStringTag, {
  value: 'ResponsePrototype',
  writable: false,
  enumerable: false,
  configurable: true
})
module.exports = Response
