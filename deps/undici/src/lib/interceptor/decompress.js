'use strict'

const { createInflate, createGunzip, createBrotliDecompress, createZstdDecompress } = require('node:zlib')
const { pipeline, Transform: TransformStream } = require('node:stream')
const { InvalidArgumentError, ResponseExceededMaxSizeError } = require('../core/errors')
const DecoratorHandler = require('../handler/decorator-handler')

/** @typedef {import('node:stream').Transform} Transform */
/** @typedef {import('node:stream').Transform} Controller */
/** @typedef {Transform&import('node:zlib').Zlib} DecompressorStream */

/** @type {Record<string, () => DecompressorStream>} */
const supportedEncodings = {
  gzip: createGunzip,
  'x-gzip': createGunzip,
  br: createBrotliDecompress,
  deflate: createInflate,
  compress: createInflate,
  'x-compress': createInflate,
  zstd: createZstdDecompress
}

const defaultSkipStatusCodes = /** @type {const} */ ([204, 304])
const defaultMaxSize = 64 * 1024 * 1024

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
 * @property {number} [maxSize=67108864] - Maximum decompressed response size in bytes
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

  constructor (handler, { skipStatusCodes = defaultSkipStatusCodes, skipErrorResponses = true, maxSize = defaultMaxSize } = {}) {
    if (!Number.isSafeInteger(maxSize) || maxSize < 1) {
      throw new InvalidArgumentError('maxSize must be a positive integer')
    }

    super(handler)
    this.#skipStatusCodes = skipStatusCodes
    this.#skipErrorResponses = skipErrorResponses
    this.#maxSize = maxSize
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
      if (i < decompressors.length - 1) {
        streams.push(createMaxSizeLimiter(this.#maxSize))
      }
    }

    return streams
  }

  /**
   * Stops decompression and reports an error.
   * @param {Controller} controller - The controller to coordinate with
   * @param {Error} error - The decompression error
   * @returns {void}
   */
  #fail (controller, error) {
    if (this.#terminated) {
      return
    }

    if (this.#inputEnded) {
      // The request is already marked complete once the compressed input ends,
      // so controller.abort() can no longer propagate decoder flush errors.
      this.onResponseError(controller, error)
    } else {
      controller.abort(error)
    }
  }

  /**
   * Sets up event handlers for a decompressor stream using readable events
   * @param {DecompressorStream} decompressor - The decompressor stream
   * @param {Controller} controller - The controller to coordinate with
   * @returns {void}
   */
  #setupDecompressorEvents (decompressor, controller) {
    decompressor.on('readable', () => {
      if (this.#terminated) {
        return
      }

      let chunk
      while ((chunk = decompressor.read()) !== null) {
        const decompressedSize = this.#decompressedSize + chunk.length
        if (decompressedSize > this.#maxSize) {
          this.#fail(controller, new ResponseExceededMaxSizeError(
            `Decompressed response size (${decompressedSize}) exceeded maxSize (${this.#maxSize})`
          ))
          return
        }

        this.#decompressedSize = decompressedSize
        const result = super.onResponseData(controller, chunk)
        if (result === false) {
          break
        }
      }
    })

    decompressor.on('error', (error) => {
      this.#fail(controller, error)
    })
  }

  /**
   * Sets up event handling for a single decompressor
   * @param {Controller} controller - The controller to handle events
   * @returns {void}
   */
  #setupSingleDecompressor (controller) {
    const decompressor = this.#decompressors[0]
    this.#setupDecompressorEvents(decompressor, controller)

    decompressor.on('end', () => {
      if (this.#terminated) {
        return
      }

      this.#terminated = true
      this.#cleanupDecompressors()
      super.onResponseEnd(controller, this.#trailers)
    })
  }

  /**
   * Sets up event handling for multiple chained decompressors using pipeline
   * @param {Controller} controller - The controller to handle events
   * @returns {void}
   */
  #setupMultipleDecompressors (controller) {
    const lastDecompressor = this.#decompressors[this.#decompressors.length - 1]
    this.#setupDecompressorEvents(lastDecompressor, controller)

    pipeline(this.#decompressors, (err) => {
      if (this.#terminated) {
        return
      }

      if (err) {
        this.#fail(controller, err)
        return
      }

      this.#terminated = true
      this.#cleanupDecompressors()
      super.onResponseEnd(controller, this.#trailers)
    })
  }

  /**
   * Cleans up decompressor references to prevent memory leaks
   * @returns {void}
   */
  #cleanupDecompressors () {
    this.#decompressors.length = 0
  }

  /**
   * @param {Controller} controller
   * @param {number} statusCode
   * @param {Record<string, string | string[] | undefined>} headers
   * @param {string} statusMessage
   * @returns {void}
   */
  onResponseStart (controller, statusCode, headers, statusMessage) {
    const contentEncoding = headers['content-encoding']

    // If content encoding is not supported or status code is in skip list
    if (this.#shouldSkipDecompression(contentEncoding, statusCode)) {
      return super.onResponseStart(controller, statusCode, headers, statusMessage)
    }

    const decompressors = this.#createDecompressionChain(contentEncoding.toLowerCase())

    if (decompressors.length === 0) {
      this.#cleanupDecompressors()
      return super.onResponseStart(controller, statusCode, headers, statusMessage)
    }

    this.#decompressors = decompressors

    // Remove compression headers since we're decompressing
    const { 'content-encoding': _, 'content-length': __, ...newHeaders } = headers

    if (controller?.rawHeaders) {
      const rawHeaders = controller.rawHeaders

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

    if (this.#decompressors.length === 1) {
      this.#setupSingleDecompressor(controller)
    } else {
      this.#setupMultipleDecompressors(controller)
    }

    return super.onResponseStart(controller, statusCode, newHeaders, statusMessage)
  }

  /**
   * @param {Controller} controller
   * @param {Buffer} chunk
   * @returns {void}
   */
  onResponseData (controller, chunk) {
    if (this.#decompressors.length > 0) {
      this.#decompressors[0].write(chunk)
      return
    }
    super.onResponseData(controller, chunk)
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
    super.onResponseEnd(controller, trailers)
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
    super.onResponseError(controller, err)
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
