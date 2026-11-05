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

/**
 * A suppressed redirect hop recorded by the redirect handler. Carried on
 * {@link kRedirectCacheControl} and inspected by the cache handler before the
 * final response is stored under the original request key.
 *
 * @typedef {object} RedirectHop
 * @property {number} statusCode The hop's response status code (e.g. 302).
 * @property {import('../../types/header.d.ts').IncomingHttpHeaders} headers The hop's response headers.
 * @property {number} receivedAt The time the hop's response headers were received.
 */
const { parseHttpDate } = require('../util/date.js')

function noop () {}

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

function appendVaryValues (target, varyValue) {
  if (Array.isArray(varyValue)) {
    for (let i = 0; i < varyValue.length; i++) {
      target.push(varyValue[i])
    }
  } else if (varyValue != null) {
    target.push(varyValue)
  }
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
 * Whether the original request carried a credential that is not reflected in
 * the cache key. The memory/sqlite stores key an entry on origin, method, path
 * and `vary` only, so a credentialed request that is redirected to a resource
 * that varies per principal must not be stored under the original (alias) key:
 * a later caller would be served the first principal's body. The set mirrors the
 * request credentials the redirect handler strips across origins (`authorization`,
 * `cookie`, `proxy-authorization`).
 *
 * @param {import('../../types/header.d.ts').IncomingHttpHeaders | undefined} headers
 * @returns {boolean}
 */
function requestCarriesCredentials (headers) {
  return headers != null && (
    Object.hasOwn(headers, 'authorization') ||
    Object.hasOwn(headers, 'cookie') ||
    Object.hasOwn(headers, 'proxy-authorization')
  )
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
   * @type {import('node:stream').Writable | undefined}
   */
  #writeStream

  /**
   * The URLs visited while following redirects, shared by reference from the
   * redirect handler through the request context. Carries the suppressed hops'
   * status/headers under {@link kRedirectCacheControl}.
   * @type {URL[] | undefined}
   */
  #history

  #replaying304 = false
  #suppress304End = false

  /**
   * @type {{ controller: import('../../types/dispatcher.d.ts').default.DispatchController, trailers: import('../../types/header.d.ts').IncomingHttpHeaders } | undefined}
   */
  #pending304End

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
    this.#replaying304 = false
    this.#suppress304End = false
    this.#pending304End = undefined
    // The redirect handler mutates this shared history as redirects are
    // followed, allowing the cache to identify the response's actual origin.
    this.#history = context?.history
    this.#handler.onRequestStart?.(controller, context)
  }

  onRequestUpgrade (controller, statusCode, headers, socket) {
    this.#handler.onRequestUpgrade?.(controller, statusCode, headers, socket)
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
        controller,
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

    // A credential-bearing request that was redirected must not have the final
    // hop's response stored under the original request's key. The cache key
    // does not include the credential, and the redirect target may vary per
    // principal (e.g. `/me` -> `/profile/{user}`), so replaying the body under
    // the alias key would leak one principal's data to another — or to an
    // unauthenticated caller — from a shared cache. The final hop's permissive
    // cache-control (e.g. `public`) does not authorize this: the credential was
    // presented to the original hop, not the resource that actually produced
    // the body. Fail closed. A history with more than one entry means at least
    // one redirect was actually followed (a single non-redirected hop records
    // exactly one entry), so a plain revalidation is not mistaken for a redirect.
    if (
      this.#history && this.#history.length > 1 &&
      requestCarriesCredentials(this.#cacheKey.headers)
    ) {
      return downstreamOnHeaders()
    }

    // A cross-origin redirect can change the origin that actually produced the
    // final response. Keying that response on the original (trusted) origin
    // would let a redirect target control an entry for a URL the application
    // trusts. Every hop in the redirect chain must stay on the cache key's
    // origin. An intermediate cross-origin hop can bounce back to the trusted
    // origin (or to any cacheable asset there) even when the final response
    // comes from the trusted origin, so checking only the final URL is not
    // enough.
    if (this.#history && this.#history.length > 0) {
      for (const hopUrl of this.#history) {
        if (!sameOrigin(hopUrl.origin, this.#cacheKey.origin)) {
          return downstreamOnHeaders()
        }
      }
    }

    // A redirect hop that is not forwarded can still carry a storage policy
    // (for example `no-store`, `private` for a shared cache, `no-cache`,
    // `must-revalidate`, zero freshness, or `Vary: *`) that the final response
    // body does not inherit. Honor it, otherwise a redirect that must not be
    // stored (or that selects a private/authenticated target) could be
    // laundered into a cacheable final response under the original key.
    const now = Date.now()
    const redirectHopInfo = this.#history?.[kRedirectCacheControl]
    const redirectPolicy = redirectHopInfo?.length > 0
      ? getRedirectPolicy(
        this.#cacheType,
        this.#cacheKey.method,
        redirectHopInfo,
        this.#cacheKey.headers,
        this.#cacheByDefault,
        now
      )
      : undefined
    if (redirectHopInfo?.length > 0 && redirectPolicy === undefined) {
      return downstreamOnHeaders()
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
    let absoluteStaleAt = staleAt + baseTime
    if (redirectPolicy !== undefined) {
      absoluteStaleAt = Math.min(absoluteStaleAt, redirectPolicy.staleAt)
    }
    if (now >= absoluteStaleAt && !revalidationOnly) {
      // Response is already stale
      deleteCachedValueIfNotModified(statusCode, this.#store, this.#cacheKey)
      return downstreamOnHeaders()
    }

    // Union the `Vary` of every suppressed redirect hop with the final
    // response's `Vary`. A 3xx hop that negotiates its target on `Authorization`
    // / `Cookie` (or any other header) must make the stored entry vary on those
    // headers too, or an authenticated user's private response would be served
    // to unrelated callers.
    let varyDirectives
    const combinedVary = []
    if (redirectHopInfo) {
      for (const hop of redirectHopInfo) {
        appendVaryValues(combinedVary, hop?.headers?.vary)
      }
    }
    appendVaryValues(combinedVary, resHeaders.vary)
    if (combinedVary.length > 0) {
      for (const varyHeader of combinedVary) {
        if (hasVaryStar(varyHeader)) {
          // `Vary: *` means the response can never be reused (RFC 9111 §4.1)
          return downstreamOnHeaders()
        }
      }
      if (this.#cacheKey.headers) {
        varyDirectives = parseVaryHeader(combinedVary, this.#cacheKey.headers)
        if (!varyDirectives) {
          // Parse error
          return downstreamOnHeaders()
        }
      }
    }

    // When the entry was folded from a redirect chain, the final response's
    // stale-serving directives (RFC 5861) must not outlive the hops' freshness.
    // A hop that grants no `stale-while-revalidate` / `stale-if-error` caps
    // stale serving at its freshness, so the final response cannot launder a
    // stale-serving window past the hop-bounded `staleAt`. Drop those directives
    // (and keep the `proxy-revalidate` the hop policy may require) so the entry
    // revalidates at `staleAt` instead of being served stale for the final
    // response's window.
    let effectiveCacheControlDirectives = cacheControlDirectives
    if (redirectPolicy !== undefined) {
      effectiveCacheControlDirectives = { ...cacheControlDirectives }
      delete effectiveCacheControlDirectives['stale-while-revalidate']
      delete effectiveCacheControlDirectives['stale-if-error']
      if (redirectPolicy.requiresRevalidation === true) {
        effectiveCacheControlDirectives['proxy-revalidate'] = true
      }
    }

    const cachedAt = baseTime
    const deleteAt = determineDeleteAt(baseTime, now, effectiveCacheControlDirectives, absoluteStaleAt)
    const strippedHeaders = stripNecessaryHeaders(resHeaders, cacheControlDirectives)

    /**
     * @type {import('../../types/cache-interceptor.d.ts').default.CacheValue}
     */
    const value = {
      statusCode,
      statusMessage,
      headers: strippedHeaders,
      vary: varyDirectives,
      cacheControlDirectives: effectiveCacheControlDirectives,
      cachedAt,
      staleAt: absoluteStaleAt,
      deleteAt
    }

    // Not modified, re-use the cached value
    // https://www.rfc-editor.org/rfc/rfc9111.html#name-handling-304-not-modified
    if (statusCode === 304) {
      this.#replaying304 = true
      let done = false
      const finish304 = () => {
        if (done) {
          return
        }
        done = true
        this.#replaying304 = false
        if (this.#pending304End) {
          const { controller, trailers } = this.#pending304End
          this.#pending304End = undefined
          this.#writeStream?.end()
          this.#handler.onResponseEnd?.(controller, trailers)
        }
      }

      // The cached body below is replayed to the downstream handler from later
      // stream ticks (or an iterator loop), outside the dispatcher's own error
      // handling, so a throwing downstream handler must be converged to a
      // per-request error instead of escaping as an uncaught exception.
      const failReplay = (err) => {
        if (done) {
          return
        }
        done = true
        this.#replaying304 = false
        this.#suppress304End = true
        this.#pending304End = undefined
        this.#writeStream?.destroy()
        // The v7 handler API has no downstream block/release controller. Keep
        // the origin response lifecycle independent, suppress its completion,
        // and report the replay failure directly downstream.
        try {
          this.#handler.onResponseError?.(controller, err)
        } catch {
          // Never let a missing legacy error callback escape this async replay.
        }
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
            try {
              for (const chunk of bodyIterator) {
                const full = this.#writeStream?.write(chunk) === false
                this.#handler.onResponseData?.(controller, chunk)
                // when stream is full stop writing until we get a 'drain' event
                if (full) {
                  return
                }
              }
              finish304()
            } catch (err) {
              failReplay(err)
            }
          }

          this.#writeStream
            .on('error', function () {
              handler.#writeStream = undefined
              handler.#store.delete(handler.#cacheKey)
              // Keep replaying downstream without the store.
              streamCachedBody()
            })
            .on('drain', () => {
              streamCachedBody()
            })
            .on('close', function () {
              if (handler.#writeStream === this) {
                handler.#writeStream = undefined
                // Keep replaying downstream without the store.
                streamCachedBody()
              }
            })

          streamCachedBody()
        } else if (typeof cachedValue.body.on === 'function') {
          // Readable stream body (e.g. from async/remote cache stores)
          cachedValue.body
            .on('data', (chunk) => {
              try {
                this.#writeStream?.write(chunk)
                this.#handler.onResponseData?.(controller, chunk)
              } catch (err) {
                cachedValue.body.destroy()
                failReplay(err)
              }
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
      if (result && typeof result.then === 'function') {
        result.then(handle304, () => handle304(undefined)).catch((err) => {
          failReplay(err)
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

    this.#handler.onResponseData?.(controller, chunk)
  }

  onResponseEnd (controller, trailers) {
    if (this.#suppress304End) {
      this.#suppress304End = false
      return
    }
    if (this.#replaying304) {
      this.#pending304End = { controller, trailers }
      return
    }
    this.#writeStream?.end()
    this.#handler.onResponseEnd?.(controller, trailers)
  }

  onResponseError (controller, err) {
    this.#replaying304 = false
    this.#pending304End = undefined
    this.#writeStream?.destroy(err)
    this.#writeStream = undefined
    this.#handler.onResponseError?.(controller, err)
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
 * Determine the absolute freshness deadline imposed by all suppressed redirect
 * hops. Returning `undefined` means at least one hop cannot safely be folded
 * into the final response under the original request key.
 *
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheOptions['type']} cacheType
 * @param {string} method
 * @param {RedirectHop[]} redirectHops
 * @param {import('../../types/header.d.ts').IncomingHttpHeaders} reqHeaders
 * @param {number | undefined} cacheByDefault
 * @param {number} now
 * @returns {{ staleAt: number, requiresRevalidation: boolean } | undefined}
 */
function getRedirectPolicy (cacheType, method, redirectHops, reqHeaders, cacheByDefault, now) {
  let staleAt = Infinity
  let requiresRevalidation = false

  for (const hop of redirectHops) {
    const hopPolicy = getRedirectHopPolicy(cacheType, method, hop, reqHeaders, cacheByDefault)
    if (hopPolicy === undefined || hopPolicy.staleAt <= now) {
      return undefined
    }

    staleAt = Math.min(staleAt, hopPolicy.staleAt)
    requiresRevalidation ||= hopPolicy.requiresRevalidation
  }

  return { staleAt, requiresRevalidation }
}

/**
 * Calculate one redirect hop's absolute freshness deadline using the same age,
 * date, expiration, invalid-directive, and heuristic rules as a final response.
 *
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheOptions['type']} cacheType
 * @param {string} method
 * @param {RedirectHop} hop
 * @param {import('../../types/header.d.ts').IncomingHttpHeaders} reqHeaders
 * @param {number | undefined} cacheByDefault
 * @returns {{ staleAt: number, requiresRevalidation: boolean } | undefined}
 */
function getRedirectHopPolicy (cacheType, method, hop, reqHeaders, cacheByDefault) {
  if (!Number.isInteger(hop?.statusCode) || !Number.isFinite(hop?.receivedAt)) {
    return undefined
  }

  const hopHeaders = hop.headers ?? {}
  const cacheControlHeader = hopHeaders['cache-control']
  const hopCacheControl = cacheControlHeader
    ? parseCacheControlHeader(cacheControlHeader)
    : {}

  // https://www.rfc-editor.org/rfc/rfc9111.html#section-4.1-5
  if (hopHeaders.vary && hasVaryStar(hopHeaders.vary)) {
    return undefined
  }

  if (hopCacheControl['no-store'] === true) {
    return undefined
  }

  // A qualified `private` directive still marks the named fields as private.
  // Folding a redirect that says `private="location"` would preserve the
  // private target choice in the final body even though a shared cache is not
  // allowed to store that Location field.
  if (cacheType === 'shared' && Object.hasOwn(hopCacheControl, 'private')) {
    return undefined
  }

  // These directives require the hop to be revalidated before reuse. Since the
  // cache stores only the final response, it cannot independently validate the
  // suppressed redirect response.
  if (
    Object.hasOwn(hopCacheControl, 'no-cache') ||
    hopCacheControl['must-revalidate'] === true ||
    (cacheType === 'shared' && hopCacheControl['proxy-revalidate'] === true)
  ) {
    return undefined
  }

  const heuristicallyCacheable =
    hopHeaders['last-modified'] &&
    arrayIncludes(HEURISTICALLY_CACHEABLE_STATUS_CODES, hop.statusCode)
  if (
    !cacheControlHeader &&
    !hopHeaders.expires &&
    !heuristicallyCacheable &&
    !cacheByDefault
  ) {
    return undefined
  }

  if (!canCacheResponse(cacheType, method, hop.statusCode, hopHeaders, hopCacheControl, reqHeaders)) {
    return undefined
  }

  const age = Object.hasOwn(hopHeaders, 'age') ? getAge(hopHeaders.age) : undefined
  if (age !== undefined && age >= MAX_RESPONSE_AGE) {
    return undefined
  }

  const responseDate = Object.hasOwn(hopHeaders, 'date') ? getDate(hopHeaders.date) : undefined
  if (responseDate === null) {
    return undefined
  }

  const apparentAge = responseDate ? Math.max(0, hop.receivedAt - responseDate.getTime()) : 0
  const currentAge = Math.max(apparentAge, age ?? 0)
  const hasValidator =
    (typeof hopHeaders.etag === 'string' && isEtagUsable(hopHeaders.etag)) ||
    typeof hopHeaders['last-modified'] === 'string'
  const freshnessLifetime =
    determineStaleAt(
      cacheType,
      hop.receivedAt,
      age,
      hopHeaders,
      responseDate,
      hopCacheControl,
      hasValidator
    ) ?? cacheByDefault

  if (freshnessLifetime === undefined) {
    return undefined
  }

  return {
    staleAt: hop.receivedAt - currentAge + freshnessLifetime,
    // RFC 9111 §5.2.2.10: s-maxage implies proxy-revalidate for shared caches.
    requiresRevalidation: cacheType === 'shared' && Object.hasOwn(hopCacheControl, 's-maxage')
  }
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
 * last-modified) that revalidation requests can be made with
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
      return sMaxAge * 1000
    }
  }

  if (hasInvalidCacheControlDirective(cacheControlDirectives, 'max-age')) {
    return 0
  }

  const maxAge = cacheControlDirectives['max-age']
  if (maxAge !== undefined) {
    return maxAge * 1000
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
    // https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.2.4
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
      // so retain it for a bounded window instead.
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
