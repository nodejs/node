'use strict'
const assert = require('node:assert')

const { kRetryHandlerDefaultRetry } = require('../core/symbols')
const { RequestRetryError } = require('../core/errors')
const {
  isDisturbed,
  parseRangeHeader,
  wrapRequestBody
} = require('../core/util')

function calculateRetryAfterHeader (retryAfter) {
  const retryTime = new Date(retryAfter).getTime()
  return isNaN(retryTime) ? 0 : retryTime - Date.now()
}

// A stable controller handed to the downstream handler for the lifetime of the
// request. Each transparent retry/resume is a *separate* dispatch with its
// *own* connection controller. Without a stable proxy the downstream body keeps
// flow-controlling the original (now-dead) controller while data flows on the
// new one: backpressure pauses the new connection's controller, but the
// consumer's resume() targets the old one, so the resumed body stalls forever.
// The proxy always forwards to the controller of the currently active connection.
class RetryController {
  constructor () {
    this.target = null
  }

  pause () { this.target?.pause() }
  resume () { this.target?.resume() }
  abort (reason) { this.target?.abort(reason) }
  get paused () { return this.target?.paused ?? false }
  get aborted () { return this.target?.aborted ?? false }
  get reason () { return this.target?.reason ?? null }
  get rawHeaders () { return this.target?.rawHeaders ?? null }
  get rawTrailers () { return this.target?.rawTrailers ?? null }
}

class RetryHandler {
  constructor (opts, { dispatch, handler }) {
    const { retryOptions, ...dispatchOpts } = opts
    const {
      // Retry scoped
      retry: retryFn,
      maxRetries,
      maxTimeout,
      minTimeout,
      timeoutFactor,
      // Response scoped
      methods,
      errorCodes,
      retryAfter,
      statusCodes,
      throwOnError
    } = retryOptions ?? {}

    this.error = null
    this.dispatch = dispatch
    this.handler = handler
    this.opts = { ...dispatchOpts, body: wrapRequestBody(opts.body) }
    this.retryOpts = {
      throwOnError: throwOnError ?? true,
      retry: retryFn ?? RetryHandler[kRetryHandlerDefaultRetry],
      retryAfter: retryAfter ?? true,
      maxTimeout: maxTimeout ?? 30 * 1000, // 30s,
      minTimeout: minTimeout ?? 500, // .5s
      timeoutFactor: timeoutFactor ?? 2,
      maxRetries: maxRetries ?? 5,
      // What errors we should retry
      methods: methods ?? ['GET', 'HEAD', 'OPTIONS', 'PUT', 'DELETE', 'TRACE', 'QUERY'],
      // Indicates which errors to retry
      statusCodes: statusCodes ?? [500, 502, 503, 504, 429],
      // List of errors to retry
      errorCodes: errorCodes ?? [
        'ECONNRESET',
        'ECONNREFUSED',
        'ENOTFOUND',
        'ENETDOWN',
        'ENETUNREACH',
        'EHOSTDOWN',
        'EHOSTUNREACH',
        'EPIPE',
        'UND_ERR_SOCKET'
      ]
    }

    this.retryCount = 0
    this.retryCountCheckpoint = 0
    this.headersSent = false
    this.start = 0
    this.end = null
    this.etag = null
    this.statusCode = null
    this.headers = null
    this.controllerProxy = new RetryController()
  }

  onResponseStartWithRetry (controller, statusCode, headers, statusMessage, err) {
    if (this.retryOpts.throwOnError) {
      // Preserve old behavior for status codes that are not eligible for retry
      if (this.retryOpts.statusCodes.includes(statusCode) === false) {
        this.headersSent = true
        this.handler.onResponseStart?.(this.controllerProxy, statusCode, headers, statusMessage)
      } else {
        this.error = err
      }

      return
    }

    if (isDisturbed(this.opts.body)) {
      this.headersSent = true
      this.handler.onResponseStart?.(this.controllerProxy, statusCode, headers, statusMessage)
      return
    }

    function shouldRetry (passedErr) {
      if (passedErr) {
        this.headersSent = true
        this.handler.onResponseStart?.(this.controllerProxy, statusCode, headers, statusMessage)
        controller.resume()
        return
      }

      this.error = err
      controller.resume()
    }

    // The pause()/resume() pair (here and in shouldRetry) acts on THIS
    // connection's controller -- never the downstream proxy. We hold this exact
    // connection while the retry policy decides (possibly after a timeout) and
    // must resume the same one. Routing this through controllerProxy would risk
    // resuming a different connection if a later dispatch re-points the proxy in
    // between, leaving this one paused forever -- the very stall the proxy exists
    // to prevent.
    controller.pause()
    this.retryOpts.retry(
      err,
      {
        state: { counter: this.retryCount },
        opts: { retryOptions: this.retryOpts, ...this.opts }
      },
      shouldRetry.bind(this)
    )
  }

  onRequestStart (controller, context) {
    // request.js creates a fresh RequestController per dispatch and passes that
    // same instance to every later callback of the dispatch. onRequestStart is
    // the first callback (it is where the controller is created), so re-pointing
    // the proxy here is enough to keep it on the active connection across every
    // transparent retry/resume.
    this.controllerProxy.target = controller
    if (!this.headersSent) {
      this.handler.onRequestStart?.(this.controllerProxy, context)
    }
  }

  onRequestUpgrade (_controller, statusCode, headers, socket) {
    this.handler.onRequestUpgrade?.(this.controllerProxy, statusCode, headers, socket)
  }

  static [kRetryHandlerDefaultRetry] (err, { state, opts }, cb) {
    const { statusCode, code, headers } = err
    const { method, retryOptions } = opts
    const {
      maxRetries,
      minTimeout,
      maxTimeout,
      timeoutFactor,
      statusCodes,
      errorCodes,
      methods
    } = retryOptions
    const { counter } = state

    // Any code that is not a Undici's originated and allowed to retry
    if (code && code !== 'UND_ERR_REQ_RETRY' && !errorCodes.includes(code)) {
      cb(err)
      return
    }

    // If a set of method are provided and the current method is not in the list
    if (Array.isArray(methods) && !methods.includes(method)) {
      cb(err)
      return
    }

    // If a set of status code are provided and the current status code is not in the list
    if (
      statusCode != null &&
      Array.isArray(statusCodes) &&
      !statusCodes.includes(statusCode)
    ) {
      cb(err)
      return
    }

    // If we reached the max number of retries
    if (counter > maxRetries) {
      cb(err)
      return
    }

    let retryAfterHeader = headers?.['retry-after']
    if (retryAfterHeader) {
      retryAfterHeader = Number(retryAfterHeader)
      retryAfterHeader = Number.isNaN(retryAfterHeader)
        ? calculateRetryAfterHeader(headers['retry-after'])
        : retryAfterHeader * 1e3 // Retry-After is in seconds
    }

    const retryTimeout =
      retryAfterHeader > 0
        ? Math.min(retryAfterHeader, maxTimeout)
        : Math.min(minTimeout * timeoutFactor ** (counter - 1), maxTimeout)

    setTimeout(() => cb(null), retryTimeout)
  }

  onResponseStart (controller, statusCode, headers, statusMessage) {
    this.error = null
    this.retryCount += 1
    this.statusCode = statusCode
    this.headers = headers

    if (statusCode >= 300) {
      const err = new RequestRetryError('Request failed', statusCode, {
        headers,
        data: {
          count: this.retryCount
        }
      })

      this.onResponseStartWithRetry(controller, statusCode, headers, statusMessage, err)
      return
    }

    // Checkpoint for resume from where we left it
    if (this.headersSent) {
      // Only Partial Content 206 supposed to provide Content-Range,
      // any other status code that partially consumed the payload
      // should not be retried because it would result in downstream
      // wrongly concatenate multiple responses.
      if (statusCode !== 206 && (this.start > 0 || statusCode !== 200)) {
        throw new RequestRetryError('server does not support the range header and the payload was partially consumed', statusCode, {
          headers,
          data: { count: this.retryCount }
        })
      }

      const contentRange = parseRangeHeader(headers['content-range'])
      // If no content range
      if (!contentRange) {
        // We always throw here as we want to indicate that we entred unexpected path
        throw new RequestRetryError('Content-Range mismatch', statusCode, {
          headers,
          data: { count: this.retryCount }
        })
      }

      // Let's start with a weak etag check
      if (this.etag != null && this.etag !== headers.etag) {
        // We always throw here as we want to indicate that we entred unexpected path
        throw new RequestRetryError('ETag mismatch', statusCode, {
          headers,
          data: { count: this.retryCount }
        })
      }

      const { start, size, end = size ? size - 1 : null } = contentRange

      assert(this.start === start, 'content-range mismatch')
      assert(this.end == null || this.end === end, 'content-range mismatch')

      return
    }

    if (this.end == null) {
      if (statusCode === 206) {
        // First time we receive 206
        const range = parseRangeHeader(headers['content-range'])

        if (range == null) {
          this.headersSent = true
          this.handler.onResponseStart?.(
            this.controllerProxy,
            statusCode,
            headers,
            statusMessage
          )
          return
        }

        const { start, size, end = size ? size - 1 : null } = range
        assert(
          start != null && Number.isFinite(start),
          'content-range mismatch'
        )
        assert(end != null && Number.isFinite(end), 'invalid content-length')

        this.start = start
        this.end = end
      }

      // We make our best to checkpoint the body for further range headers
      if (this.end == null) {
        const contentLength = headers['content-length']
        this.end = contentLength != null ? Number(contentLength) - 1 : null
      }

      assert(Number.isFinite(this.start))
      assert(
        this.end == null || Number.isFinite(this.end),
        'invalid content-length'
      )

      this.resume = true
      this.etag = headers.etag != null ? headers.etag : null

      // Weak etags are not useful for comparison nor cache
      // for instance not safe to assume if the response is byte-per-byte
      // equal
      if (
        this.etag != null &&
        this.etag[0] === 'W' &&
        this.etag[1] === '/'
      ) {
        this.etag = null
      }

      this.headersSent = true
      this.handler.onResponseStart?.(
        this.controllerProxy,
        statusCode,
        headers,
        statusMessage
      )
    } else {
      throw new RequestRetryError('Request failed', statusCode, {
        headers,
        data: { count: this.retryCount }
      })
    }
  }

  onResponseData (_controller, chunk) {
    if (this.error) {
      return
    }

    this.start += chunk.length

    this.handler.onResponseData?.(this.controllerProxy, chunk)
  }

  onResponseEnd (_controller, trailers) {
    if (this.error && this.retryOpts.throwOnError) {
      throw this.error
    }

    if (!this.error) {
      // Verify that the received body length matches the expected range
      // when we have a finite end position (from Content-Length or Content-Range)
      if (this.end != null && Number.isFinite(this.end)) {
        if (this.start !== this.end + 1) {
          throw new RequestRetryError('Content-Range mismatch', this.statusCode, {
            headers: this.headers,
            data: { count: this.retryCount }
          })
        }
      }
      this.retryCount = 0
      return this.handler.onResponseEnd?.(this.controllerProxy, trailers)
    }

    this.retry()
  }

  retry () {
    if (this.start !== 0) {
      const headers = { range: `bytes=${this.start}-${this.end ?? ''}` }

      // Weak etag check - weak etags will make comparison algorithms never match
      if (this.etag != null) {
        headers['if-match'] = this.etag
      }

      this.opts = {
        ...this.opts,
        headers: {
          ...this.opts.headers,
          ...headers
        }
      }
    }

    try {
      this.retryCountCheckpoint = this.retryCount
      this.dispatch(this.opts, this)
    } catch (err) {
      this.handler.onResponseError?.(this.controllerProxy, err)
    }
  }

  onResponseError (controller, err) {
    // controller is THIS failed connection (not the proxy): we inspect whether
    // the consumer aborted it to decide retry-vs-propagate.
    if (controller?.aborted || isDisturbed(this.opts.body)) {
      this.handler.onResponseError?.(this.controllerProxy, err)
      return
    }

    function shouldRetry (returnedErr) {
      if (!returnedErr) {
        this.retry()
        return
      }

      this.handler?.onResponseError?.(this.controllerProxy, returnedErr)
    }

    // We reconcile in case of a mix between network errors
    // and server error response
    if (this.retryCount - this.retryCountCheckpoint > 0) {
      // We count the difference between the last checkpoint and the current retry count
      this.retryCount =
        this.retryCountCheckpoint +
        (this.retryCount - this.retryCountCheckpoint)
    } else {
      this.retryCount += 1
    }

    this.retryOpts.retry(
      err,
      {
        state: { counter: this.retryCount },
        opts: { retryOptions: this.retryOpts, ...this.opts }
      },
      shouldRetry.bind(this)
    )
  }
}

module.exports = RetryHandler
