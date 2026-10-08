'use strict'

const { createInflate, createGunzip, createBrotliDecompress, createZstdDecompress } = require('node:zlib')
const { pipeline, Transform: TransformStream } = require('node:stream')
const { InvalidArgumentError, ResponseExceededMaxSizeError } = require('../core/errors')
const DecoratorHandler = require('../handler/decorator-handler')

/** @typedef {import('node:stream').Transform} Transform */
/** @typedef {import('node:stream').Transform} Controller */
/** @typedef {Transform&import('node:zlib').Zlib} DecompressorStream */

class DecompressController {
  #onPause
  #onResume
  #onAbort
  #paused = false

  constructor (onPause, onResume, onAbort) {
    this.#onPause = onPause
    this.#onResume = onResume
    this.#onAbort = onAbort
    this.target = null
  }

  pause () {
    if (this.#paused) {
      return
    }

    this.#paused = true
    this.#onPause()
  }

  resume () {
    if (!this.#paused) {
      return
    }

    this.#paused = false
    this.#onResume()
  }

  abort (reason) {
    this.target?.abort(reason)
    this.#onAbort(reason)
  }

  get paused () { return this.#paused }
  get aborted () { return this.target?.aborted ?? false }
  get reason () { return this.target?.reason ?? null }
  get rawHeaders () { return this.target?.rawHeaders ?? null }
  set rawHeaders (value) {
    if (this.target) {
      this.target.rawHeaders = value
    }
  }

  get rawTrailers () { return this.target?.rawTrailers ?? null }
  set rawTrailers (value) {
    if (this.target) {
      this.target.rawTrailers = value
    }
  }
}

/** @type {Record<string, () => DecompressorStream>} */
const supportedEncodings = {
  __proto__: null,
  gzip: createGunzip,
  'x-gzip': createGunzip,
  br: createBrotliDecompress,
  deflate: createInflate,
  compress: createInflate,
  'x-compress': createInflate,
  zstd: createZstdDecompress
}

const defaultSkipStatusCodes = /** @type {const} */ ([204, 304])
const defaultMaxSize = 0

/**
 * Limits the output of one stage in a decompression chain.
 * @param {number} maxSize - Maximum output size in bytes
 * @returns {Transform}
 */
function createMaxSizeLimiter (maxSize) {
  let size = 0

  return new TransformStream({
    transform (chunk, _encoding, callback) {
      const decompressedSize = size + chunk.length
      if (decompressedSize > maxSize) {
        callback(new ResponseExceededMaxSizeError(
          `Decompressed response size (${decompressedSize}) exceeded maxSize (${maxSize})`
        ))
        return
      }

      size = decompressedSize
      callback(null, chunk)
    }
  })
}

let warningEmitted = /** @type {boolean} */ (false)

/**
 * @typedef {Object} DecompressHandlerOptions
 * @property {number[]|Readonly<number[]>} [skipStatusCodes=[204, 304]] - List of status codes to skip decompression for
 * @property {boolean} [skipErrorResponses] - Whether to skip decompression for error responses (status codes >= 400)
 * @property {number} [maxSize=0] - Maximum decompressed response size in bytes. 0 disables the limit
 */

class DecompressHandler extends DecoratorHandler {
  /** @type {Transform[]} */
  #decompressors = []
  /** @type {Record<string, string | string[]> | undefined} */
  #trailers
  /** @type {Readonly<number[]>} */
  #skipStatusCodes
  /** @type {boolean} */
  #skipErrorResponses
  /** @type {number} */
  #maxSize
  /** @type {number} */
  #decompressedSize = 0
  /** @type {boolean} */
  #terminated = false
  /** @type {boolean} */
  #inputEnded = false
  /** @type {boolean} */
  #inputBackpressured = false
  /** @type {boolean} */
  #upstreamPaused = false
  /** @type {boolean} */
  #draining = false
  /** @type {boolean} */
  #drainRequested = false
  /** @type {boolean} */
  #completionPending = false
  /** @type {DecompressorStream | undefined} */
  #finalDecompressor
  /** @type {DecompressController} */
  #controller

  constructor (handler, { skipStatusCodes = defaultSkipStatusCodes, skipErrorResponses = true, maxSize = defaultMaxSize } = {}) {
    if (!Number.isSafeInteger(maxSize) || maxSize < 0) {
      throw new InvalidArgumentError('maxSize must be a non-negative integer')
    }

    super(handler)
    this.#skipStatusCodes = skipStatusCodes
    this.#skipErrorResponses = skipErrorResponses
    this.#maxSize = maxSize
    this.#controller = new DecompressController(
      () => this.#onDownstreamPause(),
      () => this.#onDownstreamResume(),
      reason => {
        if (this.#inputEnded && !this.#terminated) {
          this.onResponseError(this.#controller, reason)
        }
      }
    )
  }

  #onDownstreamPause () {
    this.#pauseUpstream()
  }

  #onDownstreamResume () {
    const drainWasDeferred = this.#draining
    this.#drainOutput()
    if (!drainWasDeferred) {
      this.#resumeUpstreamIfNeeded()
      this.#finishIfReady()
    }
  }

  #pauseUpstream () {
    if (!this.#upstreamPaused && !this.#terminated) {
      this.#upstreamPaused = true
      this.#controller.target?.pause()
    }
  }

  #resumeUpstreamIfNeeded () {
    if (this.#upstreamPaused && !this.#controller.paused && !this.#inputBackpressured) {
      this.#upstreamPaused = false
      if (!this.#inputEnded) {
        this.#controller.target?.resume()
      }
    }
  }

  #drainOutput () {
    if (this.#terminated || this.#controller.paused || !this.#finalDecompressor) {
      return
    }

    if (this.#draining) {
      this.#drainRequested = true
      return
    }

    this.#draining = true
    try {
      do {
        this.#drainRequested = false
        let chunk
        while (!this.#terminated && !this.#controller.paused && (chunk = this.#finalDecompressor.read()) !== null) {
          if (this.#maxSize > 0) {
            const decompressedSize = this.#decompressedSize + chunk.length
            if (decompressedSize > this.#maxSize) {
              this.#fail(new ResponseExceededMaxSizeError(
                `Decompressed response size (${decompressedSize}) exceeded maxSize (${this.#maxSize})`
              ))
              return
            }

            this.#decompressedSize = decompressedSize
          }

          const result = super.onResponseData(this.#controller, chunk)
          if (result === false && !this.#controller.paused) {
            this.#controller.pause()
          }
        }
      } while (this.#drainRequested && !this.#terminated && !this.#controller.paused)
    } finally {
      this.#draining = false
    }

    this.#resumeUpstreamIfNeeded()
    this.#finishIfReady()
  }

  #finishIfReady () {
    if (this.#terminated || !this.#completionPending || this.#controller.paused || this.#draining) {
      return
    }

    this.#terminated = true
    this.#cleanupDecompressors()
    super.onResponseEnd(this.#controller, this.#trailers)
  }

  #onDecompressionEnd () {
    if (this.#terminated) {
      return
    }

    this.#completionPending = true
    this.#drainOutput()
    this.#finishIfReady()
  }

  /**
   * Determines if decompression should be skipped based on encoding and status code
   * @param {string} contentEncoding - Content-Encoding header value
   * @param {number} statusCode - HTTP status code of the response
   * @returns {boolean} - True if decompression should be skipped
   */
  #shouldSkipDecompression (contentEncoding, statusCode) {
    if (!contentEncoding || statusCode < 200) return true
    if (this.#skipStatusCodes.includes(statusCode)) return true
    if (this.#skipErrorResponses && statusCode >= 400) return true
    return false
  }

  /**
   * Creates a chain of decompressors for multiple content encodings
   *
   * @param {string} encodings - Comma-separated list of content encodings
   * @returns {Array<Transform>} - Array of decompressor and limiting streams
   * @throws {Error} - If the number of content-encodings exceeds the maximum allowed
   */
  #createDecompressionChain (encodings) {
    const parts = encodings.split(',')

    // Limit the number of content-encodings to prevent resource exhaustion.
    // CVE fix similar to urllib3 (GHSA-gm62-xv2j-4w53) and curl (CVE-2022-32206).
    const maxContentEncodings = 5
    if (parts.length > maxContentEncodings) {
      throw new Error(`too many content-encodings in response: ${parts.length}, maximum allowed is ${maxContentEncodings}`)
    }

    /** @type {DecompressorStream[]} */
    const decompressors = []

    for (let i = parts.length - 1; i >= 0; i--) {
      const encoding = parts[i].trim()
      if (!encoding) continue

      if (!supportedEncodings[encoding]) {
        decompressors.length = 0 // Clear if unsupported encoding
        return decompressors // Unsupported encoding
      }

      decompressors.push(supportedEncodings[encoding]())
    }

    if (decompressors.length < 2) {
      return decompressors
    }

    /** @type {Transform[]} */
    const streams = []
    for (let i = 0; i < decompressors.length; i++) {
      streams.push(decompressors[i])
      if (i < decompressors.length - 1 && this.#maxSize > 0) {
        streams.push(createMaxSizeLimiter(this.#maxSize))
      }
    }

    return streams
  }

  /**
   * Stops decompression and reports an error.
   * @param {Error} error - The decompression error
   * @returns {void}
   */
  #fail (error) {
    if (this.#terminated) {
      return
    }

    if (this.#inputEnded) {
      // The request is already marked complete once the compressed input ends,
      // so controller.abort() can no longer propagate decoder flush errors.
      this.onResponseError(this.#controller, error)
    } else {
      this.#controller.abort(error)
    }
  }

  /**
   * Sets up event handlers for the final decompressor stream.
   * @param {DecompressorStream} decompressor - The decompressor stream
   * @returns {void}
   */
  #setupDecompressorEvents (decompressor) {
    this.#finalDecompressor = decompressor
    decompressor.on('readable', () => this.#drainOutput())
    decompressor.on('error', (error) => this.#fail(error))
  }

  /**
   * Sets up event handling for a single decompressor
   * @returns {void}
   */
  #setupSingleDecompressor () {
    const decompressor = this.#decompressors[0]
    this.#setupDecompressorEvents(decompressor)

    decompressor.on('end', () => this.#onDecompressionEnd())
  }

  /**
   * Sets up event handling for multiple chained decompressors using pipeline
   * @returns {void}
   */
  #setupMultipleDecompressors () {
    const lastDecompressor = this.#decompressors[this.#decompressors.length - 1]
    this.#setupDecompressorEvents(lastDecompressor)

    pipeline(this.#decompressors, (err) => {
      if (this.#terminated) {
        return
      }

      if (err) {
        this.#fail(err)
        return
      }

      this.#onDecompressionEnd()
    })
  }

  #setupInputBackpressure () {
    const decompressor = this.#decompressors[0]
    decompressor.on('drain', () => {
      if (this.#terminated) {
        return
      }

      this.#inputBackpressured = false
      if (!this.#controller.paused) {
        this.#drainOutput()
        this.#resumeUpstreamIfNeeded()
      }
    })
  }

  /**
   * Cleans up decompressor references to prevent memory leaks
   * @returns {void}
   */
  #cleanupDecompressors () {
    this.#decompressors.length = 0
    this.#finalDecompressor = undefined
  }

  onRequestStart (controller, context) {
    this.#controller.target = controller
    return super.onRequestStart(this.#controller, context)
  }

  onRequestUpgrade (controller, statusCode, headers, socket) {
    return super.onRequestUpgrade(this.#controller, statusCode, headers, socket)
  }

  /**
   * @param {Controller} controller
   * @param {number} statusCode
   * @param {Record<string, string | string[] | undefined>} headers
   * @param {string} statusMessage
   * @returns {void}
   */
  onResponseStart (controller, statusCode, headers, statusMessage) {
    // Repeated field lines reach us as an array. RFC 9110 section 5.3 lets a
    // recipient join them with commas, which yields the single-line form the
    // decompression chain already handles.
    const rawContentEncoding = headers['content-encoding']
    const contentEncoding = Array.isArray(rawContentEncoding)
      ? rawContentEncoding.join(',')
      : rawContentEncoding

    // If content encoding is not supported or status code is in skip list
    if (this.#shouldSkipDecompression(contentEncoding, statusCode)) {
      return super.onResponseStart(this.#controller, statusCode, headers, statusMessage)
    }

    const decompressors = this.#createDecompressionChain(contentEncoding.toLowerCase())

    if (decompressors.length === 0) {
      this.#cleanupDecompressors()
      return super.onResponseStart(this.#controller, statusCode, headers, statusMessage)
    }

    this.#decompressors = decompressors

    // Remove compression headers since we're decompressing
    const { 'content-encoding': _, 'content-length': __, ...newHeaders } = headers

    if (this.#controller.rawHeaders) {
      const rawHeaders = this.#controller.rawHeaders

      if (Array.isArray(rawHeaders)) {
        const filteredHeaders = []
        for (let i = 0; i < rawHeaders.length; i += 2) {
          const headerName = rawHeaders[i]
          const name = Buffer.isBuffer(headerName) ? headerName.toString('latin1') : `${headerName}`
          const lowerName = name.toLowerCase()

          if (lowerName === 'content-encoding' || lowerName === 'content-length') {
            continue
          }

          filteredHeaders.push(rawHeaders[i], rawHeaders[i + 1])
        }
        rawHeaders.splice(0, rawHeaders.length, ...filteredHeaders)
      } else if (typeof rawHeaders === 'object') {
        for (const name of Object.keys(rawHeaders)) {
          const lowerName = name.toLowerCase()
          if (lowerName === 'content-encoding' || lowerName === 'content-length') {
            delete rawHeaders[name]
          }
        }
      }
    }

    this.#setupInputBackpressure()
    if (this.#decompressors.length === 1) {
      this.#setupSingleDecompressor()
    } else {
      this.#setupMultipleDecompressors()
    }

    return super.onResponseStart(this.#controller, statusCode, newHeaders, statusMessage)
  }

  /**
   * @param {Controller} controller
   * @param {Buffer} chunk
   * @returns {void}
   */
  onResponseData (controller, chunk) {
    if (this.#decompressors.length > 0) {
      if (!this.#decompressors[0].write(chunk)) {
        this.#inputBackpressured = true
        this.#pauseUpstream()
      }
      return
    }
    return super.onResponseData(this.#controller, chunk)
  }

  /**
   * @param {Controller} controller
   * @param {Record<string, string | string[]> | undefined} trailers
   * @returns {void}
   */
  onResponseEnd (controller, trailers) {
    if (this.#decompressors.length > 0) {
      this.#inputEnded = true
      this.#trailers = trailers
      this.#decompressors[0].end()
      return
    }
    return super.onResponseEnd(this.#controller, trailers)
  }

  /**
   * @param {Controller} controller
   * @param {Error} err
   * @returns {void}
   */
  onResponseError (controller, err) {
    if (this.#terminated) {
      return
    }

    this.#terminated = true
    for (const decompressor of this.#decompressors) {
      decompressor.destroy()
    }
    this.#cleanupDecompressors()
    super.onResponseError(this.#controller, err)
  }
}

/**
 * Creates a decompression interceptor for HTTP responses
 * @param {DecompressHandlerOptions} [options] - Options for the interceptor
 * @returns {Function} - Interceptor function
 */
function createDecompressInterceptor (options = {}) {
  // Emit experimental warning only once
  if (!warningEmitted) {
    process.emitWarning(
      'DecompressInterceptor is experimental and subject to change',
      'ExperimentalWarning'
    )
    warningEmitted = true
  }

  return (dispatch) => {
    return (opts, handler) => {
      if (opts.method === 'HEAD') {
        return dispatch(opts, handler)
      }

      const decompressHandler = new DecompressHandler(handler, options)
      return dispatch(opts, decompressHandler)
    }
  }
}

module.exports = createDecompressInterceptor
