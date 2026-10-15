'use strict'

const util = require('../core/util')
const { kRedirectCacheControl } = require('../core/symbols')
const {
  parseCacheControlHeader,
  hasInvalidCacheControlDirective,
  parseVaryHeader,
  hasVaryStar,
  isInvalidOrWildcardVaryHeader,
  isEtagUsable
} = require('../util/cache')
const { parseHttpDate } = require('../util/date.js')

function noop () {}

class CacheController {
  #blocked = false
  #downstreamPaused = false

  constructor (target) {
    this.target = target
  }

  block () {
    this.#blocked = true
  }

  release () {
    this.#blocked = false
    if (!this.#downstreamPaused) {
      this.target.resume()
    }
  }

  pause () {
    this.#downstreamPaused = true
    this.target.pause()
  }

  resume () {
    this.#downstreamPaused = false
    if (!this.#blocked) {
      this.target.resume()
    }
  }

  abort (reason) {
    this.target.abort(reason)
  }

  get paused () { return this.#downstreamPaused }
  get aborted () { return this.target.aborted }
  get reason () { return this.target.reason }
  get rawHeaders () { return this.target.rawHeaders }
  set rawHeaders (value) { this.target.rawHeaders = value }
  get rawTrailers () { return this.target.rawTrailers }
  set rawTrailers (value) { this.target.rawTrailers = value }
}

/**
 * @param {string | URL} a
 * @param {string | URL} b
 * @returns {boolean}
 */
function sameOrigin (a, b) {
  try {
    return new URL(String(a)).origin === new URL(String(b)).origin
  } catch {
    return String(a) === String(b)
  }
}

// Status codes that we can use some heuristics on to cache
const HEURISTICALLY_CACHEABLE_STATUS_CODES = [
  200, 203, 204, 206, 300, 301, 308, 404, 405, 410, 414, 501
]

// Status codes which semantic is not handled by the cache
// https://datatracker.ietf.org/doc/html/rfc9111#section-3
// This list should not grow beyond 206 unless the RFC is updated
// by a newer one including more. Please introduce another list if
// implementing caching of responses with the 'must-understand' directive.
const NOT_UNDERSTOOD_STATUS_CODES = [
  206
]

const MAX_RESPONSE_AGE = 2147483647000

// Retention for revalidation-only entries (zero freshness lifetime but a
// validator present); each successful revalidation re-stores the entry.
const REVALIDATION_ONLY_RETENTION = 86400000 // 24 hours

function trimOWS (value) {
  return value.replace(/^[\t ]+|[\t ]+$/g, '')
}

function arrayIncludes (array, value) {
  for (let i = 0; i < array.length; i++) {
    if (array[i] === value) {
      return true
    }
  }

  return false
}

function appendConnectionHeaderTokens (headersToRemove, connectionHeader) {
  const values = Array.isArray(connectionHeader) ? connectionHeader : [connectionHeader]

  for (let i = 0; i < values.length; i++) {
    const tokens = values[i].split(',')
    for (let j = 0; j < tokens.length; j++) {
      headersToRemove.push(trimOWS(tokens[j]).toLowerCase())
    }
  }
}

function getSameOriginPath (cacheKey, location) {
  if (typeof location !== 'string') {
    return undefined
  }

  let originUrl
  let requestUrl
  let locationUrl
  try {
    originUrl = new URL(cacheKey.origin)
    requestUrl = new URL(cacheKey.path, originUrl)
    locationUrl = new URL(location, requestUrl)
  } catch {
    return undefined
  }

  if (locationUrl.origin !== originUrl.origin) {
    return undefined
  }

  return locationUrl.pathname + locationUrl.search
}

function deleteCachedUri (store, cacheKey, path) {
  deleteCachedValue(store, {
    ...cacheKey,
    path
  })

  for (let i = 0; i < util.safeHTTPMethods.length; i++) {
    const method = util.safeHTTPMethods[i]
    if (method !== cacheKey.method) {
      deleteCachedValue(store, {
        ...cacheKey,
        method,
        path
      })
    }
  }
}

function deleteLocationTargets (store, cacheKey, headerValue) {
  if (headerValue === undefined) {
    return
  }

  const values = Array.isArray(headerValue) ? headerValue : [headerValue]
  for (let i = 0; i < values.length; i++) {
    const path = getSameOriginPath(cacheKey, values[i])
    if (path !== undefined) {
      deleteCachedUri(store, cacheKey, path)
    }
  }
}

function invalidateUnsafeRequest (store, cacheKey, resHeaders) {
  deleteCachedUri(store, cacheKey, cacheKey.path)
  deleteLocationTargets(store, cacheKey, resHeaders.location)
  deleteLocationTargets(store, cacheKey, resHeaders['content-location'])
}
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
   * @type {CacheController | undefined}
   */
  #downstreamController

  /**
   * @type {import('node:stream').Writable | undefined}
   */
  #writeStream

  /**
   * The URLs visited while following redirects, shared by reference from the
   * redirect handler through the request context. Carries the suppressed hops'
   * cache-control under {@link kRedirectCacheControl}.
   * @type {URL[] | undefined}
   */
  #history

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
    this.#downstreamController = new CacheController(controller)
    // The redirect handler threads the list of visited URLs through the
    // handler-chain context. Keep a reference to it (it is mutated as redirects
    // are followed) so onResponseStart can tell which request actually produced
    // the response. Unlike the request options object, this reference is not
    // broken by an interceptor that copies the options between the cache and
    // the redirect.
    this.#history = context?.history
    this.#handler.onRequestStart?.(this.#downstreamController, context)
  }

  onBodySent (chunk) {
    this.#handler.onBodySent?.(chunk)
  }

  onRequestSent () {
    this.#handler.onRequestSent?.()
  }

  onRequestUpgrade (_controller, statusCode, headers, socket) {
    this.#handler.onRequestUpgrade?.(this.#downstreamController, statusCode, headers, socket)
  }

  /**
   * @param {import('../../types/dispatcher.d.ts').default.DispatchController} controller
   * @param {number} statusCode
   * @param {import('../../types/header.d.ts').IncomingHttpHeaders} resHeaders
   * @param {string} statusMessage
   */
  onResponseStart (
    controller,
    statusCode,
    resHeaders,
    statusMessage
  ) {
    const downstreamOnHeaders = () =>
      this.#handler.onResponseStart?.(
        this.#downstreamController,
        statusCode,
        resHeaders,
        statusMessage
      )
    const handler = this

    if (
      !arrayIncludes(util.safeHTTPMethods, this.#cacheKey.method) &&
      statusCode >= 200 &&
      statusCode <= 399
    ) {
      // Successful response to an unsafe method, delete it from cache
      //  https://www.rfc-editor.org/rfc/rfc9111.html#name-invalidating-stored-response
      invalidateUnsafeRequest(this.#store, this.#cacheKey, resHeaders)
      return downstreamOnHeaders()
    }

    // A cross-origin redirect can change the origin that actually produced the
    // final response. Keying that response on the original (trusted) origin
    // would let a redirect target control an entry for a URL the application
    // trusts. The final request is the last URL in the redirect history; only
    // cache when its origin still matches the cache key origin.
    if (this.#history && this.#history.length > 0) {
      const finalUrl = this.#history[this.#history.length - 1]
      if (!sameOrigin(finalUrl.origin, this.#cacheKey.origin)) {
        return downstreamOnHeaders()
      }
    }

    // A redirect hop that is not forwarded can still carry a restrictive
    // storage policy (for example `no-store`, or `private` for a shared cache).
    // Honor it, otherwise a redirect that must not be stored could be laundered
    // into a cacheable final response under the original key.
    const redirectCacheControl = this.#history?.[kRedirectCacheControl]
    if (redirectCacheControl) {
      for (const hopCacheControl of redirectCacheControl) {
        const hopDirectives = hopCacheControl ? parseCacheControlHeader(hopCacheControl) : {}
        if (
          hopDirectives['no-store'] === true ||
          (this.#cacheType === 'shared' && hopDirectives.private === true)
        ) {
          return downstreamOnHeaders()
        }
      }
    }

    const cacheControlHeader = resHeaders['cache-control']
    const cacheControlDirectives = cacheControlHeader ? parseCacheControlHeader(cacheControlHeader) : {}

    if (revalidationResponseDisallowsCachedReuse(this.#cacheType, resHeaders, cacheControlDirectives)) {
      deleteCachedValue(this.#store, this.#cacheKey)
      return downstreamOnHeaders()
    }

    const heuristicallyCacheable = resHeaders['last-modified'] && arrayIncludes(HEURISTICALLY_CACHEABLE_STATUS_CODES, statusCode)
    if (
      !cacheControlHeader &&
      !resHeaders['expires'] &&
      !heuristicallyCacheable &&
      !this.#cacheByDefault
    ) {
      if (statusCode === 304 && resHeaders.vary && isInvalidOrWildcardVaryHeader(resHeaders.vary)) {
        deleteCachedValue(this.#store, this.#cacheKey)
      }

      // Don't have anything to tell us this response is cachable and we're not
      //  caching by default
      return downstreamOnHeaders()
    }

    if (!canCacheResponse(this.#cacheType, this.#cacheKey.method, statusCode, resHeaders, cacheControlDirectives, this.#cacheKey.headers)) {
      if (statusCode === 304 && (cacheControlHeader || revalidationResponseDisallowsCachedReuse(this.#cacheType, resHeaders, cacheControlDirectives))) {
        deleteCachedValue(this.#store, this.#cacheKey)
      }

      return downstreamOnHeaders()
    }

    const now = Date.now()
    const resAge = Object.hasOwn(resHeaders, 'age') ? getAge(resHeaders.age) : undefined
    if (resAge !== undefined && resAge >= MAX_RESPONSE_AGE) {
      // Response considered stale
      deleteCachedValueIfNotModified(statusCode, this.#store, this.#cacheKey)
      return downstreamOnHeaders()
    }

    const resDate = Object.hasOwn(resHeaders, 'date') ? getDate(resHeaders.date) : undefined
    if (resDate === null) {
      deleteCachedValueIfNotModified(statusCode, this.#store, this.#cacheKey)
      return downstreamOnHeaders()
    }

    const apparentAge = resDate ? Math.max(0, now - resDate.getTime()) : 0
    const currentAge = Math.max(apparentAge, resAge ?? 0)

    const hasValidator =
      (typeof resHeaders.etag === 'string' && isEtagUsable(resHeaders.etag)) ||
      typeof resHeaders['last-modified'] === 'string'

    const staleAt =
      determineStaleAt(this.#cacheType, now, resAge, resHeaders, resDate, cacheControlDirectives, hasValidator) ??
      this.#cacheByDefault
    // Zero freshness lifetime but a validator: stale from the start, yet still
    // storable since each reuse is preceded by a revalidation request.
    // https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.2.4
    const revalidationOnly = staleAt === 0 && hasValidator
    if (staleAt === undefined || (currentAge >= staleAt && !revalidationOnly)) {
      if (cacheControlHeader || staleAt !== undefined) {
        deleteCachedValueIfNotModified(statusCode, this.#store, this.#cacheKey)
      }

      return downstreamOnHeaders()
    }

    const baseTime = now - currentAge
    const absoluteStaleAt = staleAt + baseTime
    if (now >= absoluteStaleAt && !revalidationOnly) {
      // Response is already stale
      deleteCachedValueIfNotModified(statusCode, this.#store, this.#cacheKey)
      return downstreamOnHeaders()
    }

    let varyDirectives
    if (this.#cacheKey.headers && resHeaders.vary) {
      varyDirectives = parseVaryHeader(resHeaders.vary, this.#cacheKey.headers)
      if (!varyDirectives) {
        // Parse error
        return downstreamOnHeaders()
      }
    }

    const cachedAt = baseTime
    const deleteAt = determineDeleteAt(baseTime, now, cacheControlDirectives, absoluteStaleAt)
    const strippedHeaders = stripNecessaryHeaders(resHeaders, cacheControlDirectives)

    /**
     * @type {import('../../types/cache-interceptor.d.ts').default.CacheValue}
     */
    const value = {
      statusCode,
      statusMessage,
      headers: strippedHeaders,
      vary: varyDirectives,
      cacheControlDirectives,
      cachedAt,
      staleAt: absoluteStaleAt,
      deleteAt
    }

    // Not modified, re-use the cached value
    // https://www.rfc-editor.org/rfc/rfc9111.html#name-handling-304-not-modified
    if (statusCode === 304) {
      let done = false
      const finish304 = () => {
        if (done) {
          return
        }
        done = true
        this.#downstreamController.release()
      }

      const handle304 = (cachedValue) => {
        if (!cachedValue) {
          // Do not create a new cache entry, as a 304 won't have a body - so cannot be cached.
          downstreamOnHeaders()
          finish304()
          return
        }

        // Re-use the cached value: statuscode, statusmessage, headers and body
        value.statusCode = cachedValue.statusCode
        value.statusMessage = cachedValue.statusMessage
        value.etag = cachedValue.etag
        value.vary = varyDirectives ?? cachedValue.vary
        value.headers = { ...cachedValue.headers, ...strippedHeaders }

        downstreamOnHeaders()

        this.#writeStream = this.#store.createWriteStream(this.#cacheKey, value)

        if (!this.#writeStream || !cachedValue?.body) {
          finish304()
          return
        }

        if (typeof cachedValue.body.values === 'function') {
          const bodyIterator = cachedValue.body.values()

          const streamCachedBody = () => {
            if (done) {
              return
            }
            for (const chunk of bodyIterator) {
              const full = this.#writeStream?.write(chunk) === false
              this.#handler.onResponseData?.(this.#downstreamController, chunk)
              // when stream is full stop writing until we get a 'drain' event
              if (full) {
                return
              }
            }
            finish304()
          }

          this.#writeStream
            .on('error', function () {
              handler.#writeStream = undefined
              handler.#store.delete(handler.#cacheKey)
              // Keep replaying downstream without the store
              streamCachedBody()
            })
            .on('drain', () => {
              streamCachedBody()
            })
            .on('close', function () {
              if (handler.#writeStream === this) {
                handler.#writeStream = undefined
                // Keep replaying downstream without the store
                streamCachedBody()
              }
            })

          streamCachedBody()
        } else if (typeof cachedValue.body.on === 'function') {
          // Readable stream body (e.g. from async/remote cache stores)
          cachedValue.body
            .on('data', (chunk) => {
              this.#writeStream?.write(chunk)
              this.#handler.onResponseData?.(this.#downstreamController, chunk)
            })
            .on('end', () => {
              this.#writeStream?.end()
              finish304()
            })
            .on('error', () => {
              this.#writeStream = undefined
              this.#store.delete(this.#cacheKey)
              finish304()
            })

          this.#writeStream
            .on('error', function () {
              handler.#writeStream = undefined
              handler.#store.delete(handler.#cacheKey)
            })
            .on('close', function () {
              if (handler.#writeStream === this) {
                handler.#writeStream = undefined
              }
            })
        } else {
          finish304()
        }
      }

      /**
       * @type {import('../../types/cache-interceptor.d.ts').default.CacheValue}
       */
      const result = this.#store.get(this.#cacheKey)
      // A 304 ends before the cached body is replayed. Keep the origin
      // response paused until replay is complete.
      this.#downstreamController.block()
      controller.pause()
      if (result && typeof result.then === 'function') {
        result.then(handle304, () => handle304(undefined)).catch((err) => {
          controller.abort(err)
        })
      } else {
        handle304(result)
      }
    } else {
      if (typeof resHeaders.etag === 'string' && isEtagUsable(resHeaders.etag)) {
        value.etag = resHeaders.etag
      }

      this.#writeStream = this.#store.createWriteStream(this.#cacheKey, value)

      if (!this.#writeStream) {
        return downstreamOnHeaders()
      }

      this.#writeStream
        .on('drain', () => controller.resume())
        .on('error', function () {
          // TODO (fix): Make error somehow observable?
          handler.#writeStream = undefined

          // Delete the value in case the cache store is holding onto state from
          //  the call to createWriteStream
          handler.#store.delete(handler.#cacheKey)
        })
        .on('close', function () {
          if (handler.#writeStream === this) {
            handler.#writeStream = undefined
          }

          // TODO (fix): Should we resume even if was paused downstream?
          controller.resume()
        })

      downstreamOnHeaders()
    }
  }

  onResponseData (controller, chunk) {
    if (this.#writeStream?.write(chunk) === false) {
      controller.pause()
    }

    this.#handler.onResponseData?.(this.#downstreamController, chunk)
  }

  onResponseEnd (controller, trailers) {
    this.#writeStream?.end()
    this.#handler.onResponseEnd?.(this.#downstreamController, trailers)
  }

  onResponseError (controller, err) {
    this.#writeStream?.destroy(err)
    this.#writeStream = undefined
    this.#handler.onResponseError?.(this.#downstreamController, err)
  }
}

/**
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheStore} store
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheKey} cacheKey
 */
function deleteCachedValue (store, cacheKey) {
  try {
    store.delete(cacheKey)?.catch?.(noop)
  } catch {
    // Fail silently
  }
}

function deleteCachedValueIfNotModified (statusCode, store, cacheKey) {
  if (statusCode === 304) {
    deleteCachedValue(store, cacheKey)
  }
}

/**
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheOptions['type']} cacheType
 * @param {import('../../types/header.d.ts').IncomingHttpHeaders} resHeaders
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 * @returns {boolean}
 */
function revalidationResponseDisallowsCachedReuse (cacheType, resHeaders, cacheControlDirectives) {
  return cacheControlDirectives['no-store'] === true ||
    (cacheType === 'shared' && (
      cacheControlDirectives.private === true ||
      Object.hasOwn(resHeaders, 'set-cookie')
    )) ||
    (resHeaders.vary ? isInvalidOrWildcardVaryHeader(resHeaders.vary) : false)
}

/**
 * @see https://www.rfc-editor.org/rfc/rfc9111.html#name-storing-responses-to-authen
 *
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheOptions['type']} cacheType
 * @param {string} method
 * @param {number} statusCode
 * @param {import('../../types/header.d.ts').IncomingHttpHeaders} resHeaders
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 * @param {import('../../types/header.d.ts').IncomingHttpHeaders} [reqHeaders]
 */
function canCacheResponse (cacheType, method, statusCode, resHeaders, cacheControlDirectives, reqHeaders) {
  if (!arrayIncludes(util.safeHTTPMethods, method)) {
    return false
  }
  // Status code must be final and understood.
  if (statusCode < 200 || arrayIncludes(NOT_UNDERSTOOD_STATUS_CODES, statusCode)) {
    return false
  }
  // Responses with neither status codes that are heuristically cacheable, nor "explicit enough" caching
  // directives, are not cacheable. "Explicit enough": see https://www.rfc-editor.org/rfc/rfc9111.html#section-3
  if (!arrayIncludes(HEURISTICALLY_CACHEABLE_STATUS_CODES, statusCode) && !resHeaders['expires'] &&
    !cacheControlDirectives.public &&
    cacheControlDirectives['max-age'] === undefined &&
    // RFC 9111: a private response directive, if the cache is not shared
    !(cacheControlDirectives.private && cacheType === 'private') &&
    !(cacheControlDirectives['s-maxage'] !== undefined && cacheType === 'shared')
  ) {
    return false
  }

  if (cacheControlDirectives['no-store']) {
    return false
  }

  if (cacheType === 'shared' && (
    cacheControlDirectives.private === true ||
    Object.hasOwn(resHeaders, 'set-cookie')
  )) {
    return false
  }

  // https://www.rfc-editor.org/rfc/rfc9111.html#section-4.1-5
  if (resHeaders.vary && hasVaryStar(resHeaders.vary)) {
    return false
  }

  // https://www.rfc-editor.org/rfc/rfc9111.html#name-storing-responses-to-authen
  if (reqHeaders != null && Object.hasOwn(reqHeaders, 'authorization')) {
    if (
      !cacheControlDirectives.public &&
      !cacheControlDirectives['s-maxage'] &&
      !cacheControlDirectives['must-revalidate']
    ) {
      return false
    }

    if (typeof reqHeaders.authorization !== 'string') {
      return false
    }

    if (
      Array.isArray(cacheControlDirectives['no-cache']) &&
      arrayIncludes(cacheControlDirectives['no-cache'], 'authorization')
    ) {
      return false
    }

    if (
      Array.isArray(cacheControlDirectives['private']) &&
      arrayIncludes(cacheControlDirectives['private'], 'authorization')
    ) {
      return false
    }
  }

  return true
}

/**
 * @param {string | string[]} dateHeader
 * @returns {Date | null | undefined}
 */
function getDate (dateHeader) {
  let dateValue = dateHeader
  if (Array.isArray(dateValue)) {
    if (dateValue.length !== 1) {
      return null
    }

    dateValue = dateValue[0]
  }

  if (typeof dateValue !== 'string') {
    return null
  }

  return parseHttpDate(dateValue)
}

/**
 * @param {string | string[]} ageHeader
 * @returns {number | undefined}
 */
function getAge (ageHeader) {
  let ageValue = ageHeader
  if (Array.isArray(ageValue)) {
    if (ageValue.length !== 1) {
      return MAX_RESPONSE_AGE
    }

    ageValue = ageValue[0]
  }

  if (typeof ageValue !== 'string' || !/^[\t ]*[0-9]+[\t ]*$/.test(ageValue)) {
    return MAX_RESPONSE_AGE
  }

  const age = BigInt(ageValue.replace(/^[\t ]+|[\t ]+$/g, ''))
  if (age >= BigInt(MAX_RESPONSE_AGE / 1000)) {
    return MAX_RESPONSE_AGE
  }

  return Number(age) * 1000
}

/**
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheOptions['type']} cacheType
 * @param {number} now
 * @param {number | undefined} age
 * @param {import('../../types/header.d.ts').IncomingHttpHeaders} resHeaders
 * @param {Date | undefined} responseDate
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 * @param {boolean} hasValidator whether the response has a validator (etag or
 *  last-modified) that revalidation requests can be made with
 *
 * @returns {number | undefined} time that the value is stale at in seconds or undefined if it shouldn't be cached
 */
function determineStaleAt (cacheType, now, age, resHeaders, responseDate, cacheControlDirectives, hasValidator) {
  if (cacheType === 'shared') {
    // Prioritize s-maxage since we're a shared cache
    //  s-maxage > max-age > Expire
    //  https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.2.10-3
    if (hasInvalidCacheControlDirective(cacheControlDirectives, 's-maxage')) {
      return 0
    }

    const sMaxAge = cacheControlDirectives['s-maxage']
    if (sMaxAge !== undefined) {
      if (sMaxAge > 0) {
        return sMaxAge * 1000
      }

      // Immediately stale, but storable if we can revalidate it before reuse.
      return 0
    }
  }

  if (hasInvalidCacheControlDirective(cacheControlDirectives, 'max-age')) {
    return 0
  }

  const maxAge = cacheControlDirectives['max-age']
  if (maxAge !== undefined) {
    if (maxAge > 0) {
      return maxAge * 1000
    }

    // Immediately stale, but storable if we can revalidate it before reuse.
    return 0
  }

  if (Object.hasOwn(resHeaders, 'expires')) {
    // https://www.rfc-editor.org/rfc/rfc9111.html#section-5.3
    if (typeof resHeaders.expires !== 'string') {
      return 0
    }

    const expiresDate = parseHttpDate(resHeaders.expires)
    if (!expiresDate) {
      return 0
    }

    if (now >= expiresDate.getTime()) {
      return 0
    }

    if (responseDate) {
      if (responseDate >= expiresDate) {
        return 0
      }

      const freshnessLifetime = expiresDate.getTime() - responseDate.getTime()
      if (age !== undefined && age >= freshnessLifetime) {
        return 0
      }

      return freshnessLifetime
    }

    return expiresDate.getTime() - now
  }

  if (typeof resHeaders['last-modified'] === 'string') {
    // https://www.rfc-editor.org/rfc/rfc9111.html#name-calculating-heuristic-fresh
    const lastModified = parseHttpDate(resHeaders['last-modified'])
    if (lastModified) {
      if (lastModified.getTime() >= now) {
        return undefined
      }

      const responseAge = now - lastModified.getTime()

      return responseAge * 0.1
    }
  }

  if (cacheControlDirectives.immutable) {
    // https://www.rfc-editor.org/rfc/rfc8246.html#section-2.2
    return 31536000000
  }

  if (cacheControlDirectives['no-cache'] === true && hasValidator) {
    // No freshness source, but a validator lets us revalidate before reuse.
    //  https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.2.4
    return 0
  }

  return undefined
}

/**
 * @param {number} baseTime
 * @param {number} cachedAt
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 * @param {number} staleAt
 */
function determineDeleteAt (baseTime, cachedAt, cacheControlDirectives, staleAt) {
  let staleWhileRevalidate = -Infinity
  let staleIfError = -Infinity
  let immutable = -Infinity

  if (cacheControlDirectives['stale-while-revalidate']) {
    staleWhileRevalidate = staleAt + (cacheControlDirectives['stale-while-revalidate'] * 1000)
  }

  if (cacheControlDirectives['stale-if-error']) {
    staleIfError = staleAt + (cacheControlDirectives['stale-if-error'] * 1000)
  }

  if (cacheControlDirectives.immutable && staleWhileRevalidate === -Infinity && staleIfError === -Infinity) {
    immutable = cachedAt + 31536000000
  }

  // When no stale directives or immutable flag, add a revalidation buffer
  // equal to the freshness lifetime so the entry survives past staleAt long
  // enough to be revalidated instead of silently disappearing.
  //
  // Response Date headers only have second precision, so baseTime can trail the
  // actual cache insertion time by up to ~1s. Pad the buffer by that bounded
  // skew so short-lived entries do not disappear exactly when they should be
  // revalidated.
  if (staleWhileRevalidate === -Infinity && staleIfError === -Infinity && immutable === -Infinity) {
    const freshnessLifetime = staleAt - baseTime
    if (freshnessLifetime <= 0) {
      // Revalidation-only entry: no freshness lifetime to size the buffer on,
      //  so retain it for a bounded window instead.
      return cachedAt + REVALIDATION_ONLY_RETENTION
    }
    const datePrecisionPadding = Math.min(Math.max(cachedAt - baseTime, 0), 1000)
    return staleAt + freshnessLifetime + datePrecisionPadding
  }

  return Math.max(staleAt, staleWhileRevalidate, staleIfError, immutable)
}

/**
 * Strips headers required to be removed in cached responses
 * @param {import('../../types/header.d.ts').IncomingHttpHeaders} resHeaders
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives} cacheControlDirectives
 * @returns {Record<string, string | string []>}
 */
function stripNecessaryHeaders (resHeaders, cacheControlDirectives) {
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

  if (resHeaders['connection']) {
    appendConnectionHeaderTokens(headersToRemove, resHeaders['connection'])
  }

  if (Array.isArray(cacheControlDirectives['no-cache'])) {
    headersToRemove.push(...cacheControlDirectives['no-cache'])
  }

  if (Array.isArray(cacheControlDirectives['private'])) {
    headersToRemove.push(...cacheControlDirectives['private'])
  }

  let strippedHeaders
  for (const headerName of headersToRemove) {
    if (Object.hasOwn(resHeaders, headerName)) {
      strippedHeaders ??= { ...resHeaders }
      delete strippedHeaders[headerName]
    }
  }

  return strippedHeaders ?? resHeaders
}

module.exports = CacheHandler
