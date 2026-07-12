'use strict'

const assert = require('node:assert')

/**
 * This takes care of revalidation requests we send to the origin. If we get
 *  a response indicating that what we have is cached (via a HTTP 304), we can
 *  continue using the cached value. Otherwise, we'll receive the new response
 *  here, which we then just pass on to the next handler (most likely a
 *  CacheHandler). Note that this assumes the proper headers were already
 *  included in the request to tell the origin that we want to revalidate the
 *  response (i.e. if-modified-since or if-none-match).
 *
 * @see https://www.rfc-editor.org/rfc/rfc9111.html#name-validation
 *
 * @implements {import('../../types/dispatcher.d.ts').default.DispatchHandler}
 */
class CacheRevalidationHandler {
  #successful = false

  /**
   * @type {((success: boolean, context?: any, statusCode?: number, headers?: import('../../types/header.d.ts').IncomingHttpHeaders) => void) | null}
   */
  #callback

  /**
   * @type {(import('../../types/dispatcher.d.ts').default.DispatchHandler)}
   */
  #handler

  #context

  /**
   * @type {boolean}
   */
  #allowErrorStatusCodes

  /**
   * @param {(success: boolean, context?: any, statusCode?: number, headers?: import('../../types/header.d.ts').IncomingHttpHeaders) => void} callback Function to call if the cached value is valid
   * @param {import('../../types/dispatcher.d.ts').default.DispatchHandlers} handler
   * @param {boolean} allowErrorStatusCodes
   */
  constructor (callback, handler, allowErrorStatusCodes) {
    if (typeof callback !== 'function') {
      throw new TypeError('callback must be a function')
    }

    this.#callback = callback
    this.#handler = handler
    this.#allowErrorStatusCodes = allowErrorStatusCodes
  }

  onRequestStart (_, context) {
    this.#successful = false
    this.#context = context
  }

  onRequestUpgrade (controller, statusCode, headers, socket) {
    this.#handler.onRequestUpgrade?.(controller, statusCode, headers, socket)
  }

  onResponseStart (
    controller,
    statusCode,
    headers,
    statusMessage
  ) {
    assert(this.#callback != null)

    // https://www.rfc-editor.org/rfc/rfc9111.html#name-handling-a-validation-respo
    // https://datatracker.ietf.org/doc/html/rfc5861#section-4
    this.#successful = statusCode === 304 ||
      (this.#allowErrorStatusCodes && statusCode >= 500 && statusCode <= 504)
    this.#callback(this.#successful, this.#context, statusCode, headers)
    this.#callback = null

    if (this.#successful) {
      return true
    }

    this.#handler.onRequestStart?.(controller, this.#context)
    this.#handler.onResponseStart?.(
      controller,
      statusCode,
      headers,
      statusMessage
    )
  }

  onResponseData (controller, chunk) {
    if (this.#successful) {
      return
    }

    return this.#handler.onResponseData?.(controller, chunk)
  }

  onResponseEnd (controller, trailers) {
    if (this.#successful) {
      return
    }

    this.#handler.onResponseEnd?.(controller, trailers)
  }

  onResponseError (controller, err) {
    if (this.#successful) {
      return
    }

    if (this.#callback) {
      // Serve the stale cached response on a connection error, per stale-if-error:
      //  RFC 5861 counts an unreachable origin (a would-be 5xx) as an error.
      // https://datatracker.ietf.org/doc/html/rfc5861#section-4
      if (this.#allowErrorStatusCodes) {
        this.#successful = true
        this.#callback(true, this.#context)
        this.#callback = null
        return
      }

      this.#callback(false)
      this.#callback = null
    }

    if (typeof this.#handler.onResponseError === 'function') {
      this.#handler.onResponseError(controller, err)
    } else {
      throw err
    }
  }
}

module.exports = CacheRevalidationHandler
