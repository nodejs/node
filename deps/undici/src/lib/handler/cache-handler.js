'use strict'

const util = require('../core/util')
const {
  parseCacheControlHeader,
  parseVaryHeader,
  isEtagUsable
} = require('../util/cache')

function noop () {}

/**
 * @typedef {import('../../types/dispatcher.d.ts').default.DispatchHandler} DispatchHandler
 *
 * @implements {DispatchHandler}
 */
class CacheHandler {
  /**
   * @type {import('../../types/cache-interceptor.d.ts').default.CacheKey}
   */
  #cacheKey

  /**
   * @type {import('../../types/cache-interceptor.d.ts').default.CacheHandlerOptions['type']}
   */
  #cacheType

  /**
   * @type {number | undefined}
   */
  #cacheByDefault

  /**
   * @type {import('../../types/cache-interceptor.d.ts').default.CacheStore}
   */
  #store

  /**
   * @type {import('../../types/dispatcher.d.ts').default.DispatchHandler}
   */
  #handler

  /**
   * @type {import('node:stream').Writable | undefined}
   */
  #writeStream

  /**
   * @param {import('../../types/cache-interceptor.d.ts').default.CacheHandlerOptions} opts
   * @param {import('../../types/cache-interceptor.d.ts').default.CacheKey} cacheKey
   * @param {import('../../types/dispatcher.d.ts').default.DispatchHandler} handler
   */
  constructor ({ store, type, cacheByDefault }, cacheKey, handler) {
    this.#store = store
    this.#cacheType = type
    this.#cacheByDefault = cacheByDefault
    this.#cacheKey = cacheKey
    this.#handler = handler
  }

  onRequestStart (controller, context) {
    this.#writeStream?.destroy()
    this.#writeStream = undefined
    this.#handler.onRequestStart?.(controller, context)
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
    const downstreamOnHeaders = () =>
      this.#handler.onResponseStart?.(
        controller,
        statusCode,
        headers,
        statusMessage
      )

    if (
      !util.safeHTTPMethods.includes(this.#cacheKey.method) &&
      statusCode >= 200 &&
      statusCode <= 399
    ) {
      // https://www.rfc-editor.org/rfc/rfc9111.html#name-invalidating-stored-response
      try {
        this.#store.delete(this.#cacheKey).catch?.(noop)
      } catch {
        // Fail silently
      }
      return downstreamOnHeaders()
    }

    const cacheControlHeader = headers['cache-control']
    if (!cacheControlHeader && !headers['expires'] && !this.#cacheByDefault) {
      // Don't have the cache control header or the cache is full
      return downstreamOnHeaders()
    }

    const cacheControlDirectives = cacheControlHeader ? parseCacheControlHeader(cacheControlHeader) : {}
    if (!canCacheResponse(this.#cacheType, statusCode, headers, cacheControlDirectives)) {
      return downstreamOnHeaders()
    }

    const age = getAge(headers)

    const now = Date.now()
    const staleAt = determineStaleAt(this.#cacheType, now, headers, cacheControlDirectives) ?? this.#cacheByDefault
    if (staleAt) {
      let baseTime = now
      if (headers['date']) {
        const parsedDate = parseInt(headers['date'])
        const date = new Date(isNaN(parsedDate) ? headers['date'] : parsedDate)
        if (date instanceof Date && !isNaN(date)) {
          baseTime = date.getTime()
        }
      }

      const absoluteStaleAt = staleAt + baseTime

      if (now >= absoluteStaleAt || (age && age >= staleAt)) {
        // Response is already stale
        return downstreamOnHeaders()
      }

      let varyDirectives
      if (this.#cacheKey.headers && headers.vary) {
        varyDirectives = parseVaryHeader(headers.vary, this.#cacheKey.headers)
        if (!varyDirectives) {
          // Parse error
          return downstreamOnHeaders()
        }
      }

      const deleteAt = determineDeleteAt(cacheControlDirectives, absoluteStaleAt)
      const strippedHeaders = stripNecessaryHeaders(headers, cacheControlDirectives)

      /**
       * @type {import('../../types/cache-interceptor.d.ts').default.CacheValue}
       */
      const value = {
        statusCode,
        statusMessage,
        headers: strippedHeaders,
        vary: varyDirectives,
        cacheControlDirectives,
        cachedAt: age ? now - (age * 1000) : now,
        staleAt: absoluteStaleAt,
        deleteAt
      }

      if (typeof headers.etag === 'string' && isEtagUsable(headers.etag)) {
        value.etag = headers.etag
      }

      this.#writeStream = this.#store.createWriteStream(this.#cacheKey, value)

      if (this.#writeStream) {
        const handler = this
        this.#writeStream
          .on('drain', () => controller.resume())
          .on('error', function () {
            // TODO (fix): Make error somehow observable?
            handler.#writeStream = undefined
          })
          .on('close', function () {
            if (handler.#writeStream === this) {
              handler.#writeStream = undefined
            }

            // TODO (fix): Should we resume even if was paused downstream?
            controller.resume()
          })
      }
    }

    return downstreamOnHeaders()
  }

  onResponseData (controller, chunk) {
    if (this.#writeStream?.write(chunk) === false) {
      controller.pause()
    }

    this.#handler.onResponseData?.(controller, chunk)
  }

  onResponseEnd (controller, trailers) {
    this.#writeStream?.end()
    this.#handler.onResponseEnd?.(controller, trailers)
  }

  onResponseError (controller, err) {
    this.#writeStream?.destroy(err)
    this.#writeStream = undefined
    this.#handler.onResponseError?.(controller, err)
  }
}

/**
 * @see https://www.rfc-editor.org/rfc/rfc9111.html#name-storing-responses-to-authen
 *
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheOptions['type']} cacheType
 * @param {number} statusCode
 * @param {Record<string, string | string[]>} headers
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 */
function canCacheResponse (cacheType, statusCode, headers, cacheControlDirectives) {
  if (statusCode !== 200 && statusCode !== 307) {
    return false
  }

  if (
    cacheControlDirectives['no-cache'] === true ||
    cacheControlDirectives['no-store']
  ) {
    return false
  }

  if (cacheType === 'shared' && cacheControlDirectives.private === true) {
    return false
  }

  // https://www.rfc-editor.org/rfc/rfc9111.html#section-4.1-5
  if (headers.vary?.includes('*')) {
    return false
  }

  // https://www.rfc-editor.org/rfc/rfc9111.html#name-storing-responses-to-authen
  if (headers.authorization) {
    if (!cacheControlDirectives.public || typeof headers.authorization !== 'string') {
      return false
    }

    if (
      Array.isArray(cacheControlDirectives['no-cache']) &&
      cacheControlDirectives['no-cache'].includes('authorization')
    ) {
      return false
    }

    if (
      Array.isArray(cacheControlDirectives['private']) &&
      cacheControlDirectives['private'].includes('authorization')
    ) {
      return false
    }
  }

  return true
}

/**
 * @param {Record<string, string | string[]>} headers
 * @returns {number | undefined}
 */
function getAge (headers) {
  if (!headers.age) {
    return undefined
  }

  const age = parseInt(Array.isArray(headers.age) ? headers.age[0] : headers.age)
  if (isNaN(age) || age >= 2147483647) {
    return undefined
  }

  return age
}

/**
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheOptions['type']} cacheType
 * @param {number} now
 * @param {Record<string, string | string[]>} headers
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 *
 * @returns {number | undefined} time that the value is stale at or undefined if it shouldn't be cached
 */
function determineStaleAt (cacheType, now, headers, cacheControlDirectives) {
  if (cacheType === 'shared') {
    // Prioritize s-maxage since we're a shared cache
    //  s-maxage > max-age > Expire
    //  https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.2.10-3
    const sMaxAge = cacheControlDirectives['s-maxage']
    if (sMaxAge) {
      return sMaxAge * 1000
    }
  }

  const maxAge = cacheControlDirectives['max-age']
  if (maxAge) {
    return maxAge * 1000
  }

  if (headers.expires && typeof headers.expires === 'string') {
    // https://www.rfc-editor.org/rfc/rfc9111.html#section-5.3
    const expiresDate = new Date(headers.expires)
    if (expiresDate instanceof Date && Number.isFinite(expiresDate.valueOf())) {
      if (now >= expiresDate.getTime()) {
        return undefined
      }

      return expiresDate.getTime() - now
    }
  }

  if (cacheControlDirectives.immutable) {
    // https://www.rfc-editor.org/rfc/rfc8246.html#section-2.2
    return 31536000
  }

  return undefined
}

/**
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 * @param {number} staleAt
 */
function determineDeleteAt (cacheControlDirectives, staleAt) {
  let staleWhileRevalidate = -Infinity
  let staleIfError = -Infinity
  let immutable = -Infinity

  if (cacheControlDirectives['stale-while-revalidate']) {
    staleWhileRevalidate = staleAt + (cacheControlDirectives['stale-while-revalidate'] * 1000)
  }

  if (cacheControlDirectives['stale-if-error']) {
    staleIfError = staleAt + (cacheControlDirectives['stale-if-error'] * 1000)
  }

  if (staleWhileRevalidate === -Infinity && staleIfError === -Infinity) {
    immutable = 31536000
  }

  return Math.max(staleAt, staleWhileRevalidate, staleIfError, immutable)
}

/**
 * Strips headers required to be removed in cached responses
 * @param {Record<string, string | string[]>} headers
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 * @returns {Record<string, string | string []>}
 */
function stripNecessaryHeaders (headers, cacheControlDirectives) {
  const headersToRemove = [
    'connection',
    'proxy-authenticate',
    'proxy-authentication-info',
    'proxy-authorization',
    'proxy-connection',
    'te',
    'transfer-encoding',
    'upgrade',
    // We'll add age back when serving it
    'age'
  ]

  if (headers['connection']) {
    if (Array.isArray(headers['connection'])) {
      // connection: a
      // connection: b
      headersToRemove.push(...headers['connection'].map(header => header.trim()))
    } else {
      // connection: a, b
      headersToRemove.push(...headers['connection'].split(',').map(header => header.trim()))
    }
  }

  if (Array.isArray(cacheControlDirectives['no-cache'])) {
    headersToRemove.push(...cacheControlDirectives['no-cache'])
  }

  if (Array.isArray(cacheControlDirectives['private'])) {
    headersToRemove.push(...cacheControlDirectives['private'])
  }

  let strippedHeaders
  for (const headerName of headersToRemove) {
    if (headers[headerName]) {
      strippedHeaders ??= { ...headers }
      delete strippedHeaders[headerName]
    }
  }

  return strippedHeaders ?? headers
}

module.exports = CacheHandler
