'use strict'

const {
  safeHTTPMethods,
  pathHasQueryOrFragment,
  hasSafeIterator,
  isValidHTTPToken
} = require('../core/util')

const { serializePathWithQuery } = require('../core/util')

const MAX_DELTA_SECONDS = 2147483647
const RESTRICTIVE_DIRECTIVE_NAMES = ['no-store', 'private', 'no-cache']
const kInvalidCacheControlDirectives = Symbol('invalid cache-control directives')

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

function trimOWSStart (value) {
  return value.replace(/^[\t ]+/, '')
}

function trimOWSEnd (value) {
  return value.replace(/[\t ]+$/, '')
}

function findUnescapedQuote (value, start) {
  let escaped = false
  for (let i = start; i < value.length; i++) {
    if (escaped) {
      escaped = false
    } else if (value[i] === '\\') {
      escaped = true
    } else if (value[i] === '"') {
      return i
    }
  }

  return -1
}

function splitCacheControlHeaderValue (value) {
  const directives = []
  let start = 0
  let quoteStart = -1
  let inQuote = false
  let escaped = false

  for (let i = 0; i < value.length; i++) {
    if (inQuote) {
      if (escaped) {
        escaped = false
      } else if (value[i] === '\\') {
        escaped = true
      } else if (value[i] === '"') {
        inQuote = false
        quoteStart = -1
      }
    } else if (value[i] === '"') {
      inQuote = true
      quoteStart = i
    } else if (value[i] === ',') {
      directives.push({ value: value.substring(start, i), fromMalformedQuote: false })
      start = i + 1
    }
  }

  if (!inQuote) {
    directives.push({ value: value.substring(start), fromMalformedQuote: false })
    return directives
  }

  const tail = value.substring(start)
  const quoteOffset = quoteStart - start
  let tailStart = 0
  for (let i = 0; i < tail.length; i++) {
    if (tail[i] === ',') {
      directives.push({
        value: tail.substring(tailStart, i),
        fromMalformedQuote: tailStart > quoteOffset
      })
      tailStart = i + 1
    }
  }

  directives.push({
    value: tail.substring(tailStart),
    fromMalformedQuote: tailStart > quoteOffset
  })
  return directives
}

function markInvalidCacheControlDirective (directives, key) {
  let invalidDirectives = directives[kInvalidCacheControlDirectives]

  if (invalidDirectives === undefined) {
    invalidDirectives = new Set()
    Object.defineProperty(directives, kInvalidCacheControlDirectives, {
      value: invalidDirectives
    })
  }

  invalidDirectives.add(key)
}

function hasInvalidCacheControlDirective (directives, key) {
  return directives[kInvalidCacheControlDirectives]?.has(key) === true
}

function getMalformedRestrictiveDirectiveName (key) {
  for (const directiveName of RESTRICTIVE_DIRECTIVE_NAMES) {
    if (
      key.startsWith(directiveName) &&
      key.length > directiveName.length &&
      !isValidHTTPToken(key[directiveName.length])
    ) {
      return directiveName
    }
  }

  let tokenOnlyKey = ''
  let hasInvalidTokenChar = false
  for (let i = 0; i < key.length; i++) {
    if (isValidHTTPToken(key[i])) {
      tokenOnlyKey += key[i]
    } else {
      hasInvalidTokenChar = true
    }
  }

  if (hasInvalidTokenChar && arrayIncludes(RESTRICTIVE_DIRECTIVE_NAMES, tokenOnlyKey)) {
    return tokenOnlyKey
  }
}

/**
 * @param {import('../../types/dispatcher.d.ts').default.DispatchOptions} opts
 */
function makeCacheKey (opts) {
  if (!opts.origin) {
    throw new Error('opts.origin is undefined')
  }

  let fullPath = opts.path || '/'

  if (opts.query && !pathHasQueryOrFragment(fullPath)) {
    fullPath = serializePathWithQuery(fullPath, opts.query)
  }

  return {
    origin: opts.origin.toString(),
    method: opts.method,
    path: fullPath,
    headers: opts.headers
  }
}

function appendHeader (headers, key, val) {
  const headerName = key.toLowerCase()
  const current = headers[headerName]
  const values = Array.isArray(val) ? val : [val]

  if (current === undefined) {
    headers[headerName] = Array.isArray(val) ? val.slice() : val
  } else if (Array.isArray(current)) {
    current.push(...values)
  } else {
    headers[headerName] = [current, ...values]
  }
}

/**
 * @param {Record<string, string[] | string>}
 * @returns {Record<string, string[] | string>}
 */
function normalizeHeaders (opts) {
  let headers
  if (opts.headers == null) {
    headers = {}
  } else if (typeof opts.headers === 'object') {
    headers = {}

    if (hasSafeIterator(opts.headers)) {
      if (Array.isArray(opts.headers)) {
        // Array format: could be flat alternating [k, v, k, v, ...]
        // or array-of-pairs [[k, v], ...]
        const first = opts.headers[0]
        if (Array.isArray(first)) {
          for (const x of opts.headers) {
            if (!Array.isArray(x)) {
              throw new Error('opts.headers is not a valid header map')
            }
            const [key, val] = x
            if (typeof key !== 'string' || typeof val !== 'string') {
              throw new Error('opts.headers is not a valid header map')
            }
            appendHeader(headers, key, val)
          }
        } else {
          // Flat alternating array [k, v, k, v, ...]
          const len = opts.headers.length
          if (len % 2 !== 0) {
            throw new Error('opts.headers is not a valid header map')
          }
          for (let i = 0; i < len; i += 2) {
            const key = opts.headers[i]
            const val = opts.headers[i + 1]
            if (typeof key !== 'string' || (typeof val !== 'string' && !Array.isArray(val))) {
              throw new Error('opts.headers is not a valid header map')
            }
            if (typeof val === 'string') {
              appendHeader(headers, key, val)
            } else {
              const mapped = []
              for (let j = 0; j < val.length; j++) {
                const v = val[j]
                mapped.push(typeof v === 'string' ? v : v.toString('latin1'))
              }
              appendHeader(headers, key, mapped)
            }
          }
        }
      } else {
        // Non-array iterable (e.g. Map) — use original iteration logic
        for (const x of opts.headers) {
          if (!Array.isArray(x)) {
            throw new Error('opts.headers is not a valid header map')
          }
          const [key, val] = x
          if (typeof key !== 'string' || typeof val !== 'string') {
            throw new Error('opts.headers is not a valid header map')
          }
          appendHeader(headers, key, val)
        }
      }
    } else {
      for (const key of Object.keys(opts.headers)) {
        appendHeader(headers, key, opts.headers[key])
      }
    }
  } else {
    throw new Error('opts.headers is not an object')
  }

  return headers
}

/**
 * @param {any} key
 */
function assertCacheKey (key) {
  if (typeof key !== 'object') {
    throw new TypeError(`expected key to be object, got ${typeof key}`)
  }

  for (const property of ['origin', 'method', 'path']) {
    if (typeof key[property] !== 'string') {
      throw new TypeError(`expected key.${property} to be string, got ${typeof key[property]}`)
    }
  }

  if (key.headers !== undefined && typeof key.headers !== 'object') {
    throw new TypeError(`expected headers to be object, got ${typeof key}`)
  }
}

/**
 * @param {any} value
 */
function assertCacheValue (value) {
  if (typeof value !== 'object') {
    throw new TypeError(`expected value to be object, got ${typeof value}`)
  }

  for (const property of ['statusCode', 'cachedAt', 'staleAt', 'deleteAt']) {
    if (typeof value[property] !== 'number') {
      throw new TypeError(`expected value.${property} to be number, got ${typeof value[property]}`)
    }
  }

  if (typeof value.statusMessage !== 'string') {
    throw new TypeError(`expected value.statusMessage to be string, got ${typeof value.statusMessage}`)
  }

  if (value.headers != null && typeof value.headers !== 'object') {
    throw new TypeError(`expected value.rawHeaders to be object, got ${typeof value.headers}`)
  }

  if (value.vary !== undefined && typeof value.vary !== 'object') {
    throw new TypeError(`expected value.vary to be object, got ${typeof value.vary}`)
  }

  if (value.etag !== undefined && typeof value.etag !== 'string') {
    throw new TypeError(`expected value.etag to be string, got ${typeof value.etag}`)
  }
}

/**
 * @see https://www.rfc-editor.org/rfc/rfc9111.html#name-cache-control
 * @see https://www.iana.org/assignments/http-cache-directives/http-cache-directives.xhtml

 * @param {string | string[]} header
 * @returns {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives}
 */
function parseCacheControlHeader (header) {
  /**
   * @type {import('../../types/cache-interceptor.d.ts').default.CacheControlDirectives}
   */
  const output = {}
  const invalidNumericDirectives = new Set()
  const invalidNoArgumentDirectives = new Set()

  const directives = splitCacheControlHeaderValue(Array.isArray(header) ? header.join(',') : header)

  for (let i = 0; i < directives.length; i++) {
    const directiveRecord = directives[i]
    const directive = directiveRecord.value.toLowerCase()
    const fromMalformedQuote = directiveRecord.fromMalformedQuote
    const keyValueDelimiter = directive.indexOf('=')

    let key
    let value
    let keyHasTrailingWhitespace = false
    let valueHasLeadingWhitespace = false
    if (keyValueDelimiter !== -1) {
      const rawKey = directive.substring(0, keyValueDelimiter)
      const rawValue = directive.substring(keyValueDelimiter + 1)

      keyHasTrailingWhitespace = trimOWSEnd(rawKey) !== rawKey
      valueHasLeadingWhitespace = trimOWSStart(rawValue) !== rawValue
      key = trimOWS(rawKey)
      value = trimOWSStart(rawValue)
    } else {
      key = trimOWS(directive)
    }

    const malformedRestrictiveDirectiveName = getMalformedRestrictiveDirectiveName(key)
    if (malformedRestrictiveDirectiveName !== undefined) {
      output[malformedRestrictiveDirectiveName] = true
      continue
    }

    switch (key) {
      case 'min-fresh':
      case 'max-stale':
      case 'max-age':
      case 's-maxage':
      case 'stale-while-revalidate':
      case 'stale-if-error': {
        if (fromMalformedQuote || invalidNumericDirectives.has(key)) {
          continue
        }

        if (value === undefined || keyHasTrailingWhitespace || valueHasLeadingWhitespace) {
          delete output[key]
          invalidNumericDirectives.add(key)
          markInvalidCacheControlDirective(output, key)
          continue
        }

        if (
          value.length >= 2 &&
          value[0] === '"' &&
          value[value.length - 1] === '"'
        ) {
          value = value.substring(1, value.length - 1)
        }

        if (!/^[0-9]+$/.test(value)) {
          delete output[key]
          invalidNumericDirectives.add(key)
          markInvalidCacheControlDirective(output, key)
          continue
        }

        const parsedValue = Math.min(parseInt(value, 10), MAX_DELTA_SECONDS)

        if (key === 'min-fresh') {
          if (!(key in output) || output[key] < parsedValue) {
            output[key] = parsedValue
          }
        } else if (!(key in output) || output[key] > parsedValue) {
          output[key] = parsedValue
        }

        break
      }
      case 'private':
      case 'no-cache': {
        if (fromMalformedQuote) {
          output[key] = true
          break
        }

        if (value !== undefined && value.length === 0) {
          output[key] = true
          break
        }

        if (value) {
          // The private and no-cache directives can be unqualified (aka just
          //  `private` or `no-cache`) or qualified (w/ a value). When they're
          //  qualified, it's a list of headers like `no-cache=header1`,
          //  `no-cache="header1"`, or `no-cache="header1, header2"`
          // If we're given multiple headers, the comma messes us up since
          //  we split the full header by commas. So, let's loop through the
          //  remaining parts in front of us until we find one that contains a
          //  closing quote. We can then skip the consumed quoted-list fragments and
          //  continue parsing like normal.
          // https://www.rfc-editor.org/rfc/rfc9111.html#name-no-cache-2
          if (value[0] === '"') {
            // Something like `no-cache="some-header"` OR `no-cache="some-header, another-header"`.
            value = trimOWSEnd(value)

            let fieldList = ''
            let lastQuotedPart = i
            let foundEndingQuote = false
            const closingQuote = findUnescapedQuote(value, 1)

            if (closingQuote !== -1) {
              fieldList = value.substring(1, closingQuote)
              foundEndingQuote = true
            } else {
              // Something like `no-cache="some-header, another-header"`
              //  This can still be something invalid, e.g. `no-cache="some-header, ...`
              const fieldListParts = [value.substring(1)]

              for (let j = i + 1; j < directives.length; j++) {
                const nextPart = trimOWS(directives[j].value)
                const closingQuote = findUnescapedQuote(nextPart, 0)

                lastQuotedPart = j

                if (closingQuote !== -1) {
                  fieldListParts.push(nextPart.substring(0, closingQuote))
                  foundEndingQuote = true
                  break
                }

                fieldListParts.push(nextPart)
              }

              fieldList = fieldListParts.join(',')
            }

            if (!foundEndingQuote) {
              output[key] = true
              break
            }

            i = lastQuotedPart

            const headers = fieldList.split(',')
            let validFieldNames = true
            for (let j = 0; j < headers.length; j++) {
              headers[j] = trimOWS(headers[j])
              if (!isValidHTTPToken(headers[j])) {
                validFieldNames = false
              }
            }

            if (!validFieldNames) {
              output[key] = true
            } else if (output[key] !== true) {
              if (key in output) {
                output[key] = output[key].concat(headers)
              } else {
                output[key] = headers
              }
            }
          } else {
            // Something like `no-cache=some-header`
            const fieldName = trimOWS(value)

            if (!isValidHTTPToken(fieldName)) {
              output[key] = true
            } else if (output[key] !== true) {
              if (key in output) {
                output[key] = output[key].concat(fieldName)
              } else {
                output[key] = [fieldName]
              }
            }
          }

          break
        }
      }
      // eslint-disable-next-line no-fallthrough
      case 'public':
      case 'must-revalidate':
      case 'proxy-revalidate':
      case 'immutable':
      case 'no-transform':
      case 'must-understand':
      case 'only-if-cached':
        if (fromMalformedQuote || invalidNoArgumentDirectives.has(key)) {
          continue
        }

        if (value !== undefined) {
          // These are qualified (something like `public=...`) when they aren't
          //  allowed to be, skip all instances of the malformed directive.
          delete output[key]
          invalidNoArgumentDirectives.add(key)
          continue
        }

        output[key] = true
        break
      case 'no-store':
        output[key] = true
        break
      default:
        // Ignore unknown directives as per https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.3-1
        continue
    }
  }

  return output
}

/**
 * @param {string | string[]} varyHeader Vary header from the server
 * @returns {string[]}
 */
function splitVaryHeader (varyHeader) {
  const values = Array.isArray(varyHeader) ? varyHeader : [varyHeader]
  const output = []

  for (let i = 0; i < values.length; i++) {
    const parts = values[i].split(',')
    for (let j = 0; j < parts.length; j++) {
      output.push(parts[j])
    }
  }

  return output
}

/**
 * @param {string | string[]} varyHeader Vary header from the server
 * @returns {boolean}
 */
function hasVaryStar (varyHeader) {
  const values = splitVaryHeader(varyHeader)
  for (let i = 0; i < values.length; i++) {
    if (trimOWS(values[i]).indexOf('*') !== -1) {
      return true
    }
  }

  return false
}

/**
 * @param {string | string[]} varyHeader Vary header from the server
 * @param {Record<string, string | string[]>} headers Request headers
 * @returns {Record<string, string | string[] | null> | undefined}
 */
function parseVaryHeader (varyHeader, headers) {
  if (hasVaryStar(varyHeader)) {
    return headers
  }

  const output = /** @type {Record<string, string | string[] | null>} */ ({})

  const varyingHeaders = splitVaryHeader(varyHeader)

  for (const header of varyingHeaders) {
    const trimmedHeader = trimOWS(header).toLowerCase()

    if (trimmedHeader.length === 0) {
      continue
    }

    if (!isValidHTTPToken(trimmedHeader)) {
      return undefined
    }

    const headerValue = headers[trimmedHeader]
    output[trimmedHeader] = Array.isArray(headerValue) ? headerValue.slice() : headerValue ?? null
  }

  return output
}

/**
 * @param {string | string[]} varyHeader Vary header from the server
 * @returns {boolean}
 */
function isInvalidOrWildcardVaryHeader (varyHeader) {
  return hasVaryStar(varyHeader) || parseVaryHeader(varyHeader, {}) === undefined
}

/**
 * Note: this deviates from the spec a little. Empty etags ("", W/"") are valid,
 *  however, including them in cached resposnes serves little to no purpose.
 *
 * @see https://www.rfc-editor.org/rfc/rfc9110.html#name-etag
 *
 * @param {string} etag
 * @returns {boolean}
 */
function isEtagUsable (etag) {
  if (etag.length <= 2) {
    // Shortest an etag can be is two chars (just ""). This is where we deviate
    //  from the spec requiring a min of 3 chars however
    return false
  }

  if (etag[0] === '"' && etag[etag.length - 1] === '"') {
    // ETag: ""asd123"" or ETag: "W/"asd123"", kinda undefined behavior in the
    //  spec. Some servers will accept these while others don't.
    // ETag: "asd123"
    return !(etag[1] === '"' || etag.startsWith('"W/'))
  }

  if (etag.startsWith('W/"') && etag[etag.length - 1] === '"') {
    // ETag: W/"", also where we deviate from the spec & require a min of 3
    //  chars
    // ETag: for W/"", W/"asd123"
    return etag.length !== 4
  }

  // Anything else
  return false
}

/**
 * @param {unknown} store
 * @returns {asserts store is import('../../types/cache-interceptor.d.ts').default.CacheStore}
 */
function assertCacheStore (store, name = 'CacheStore') {
  if (typeof store !== 'object' || store === null) {
    throw new TypeError(`expected type of ${name} to be a CacheStore, got ${store === null ? 'null' : typeof store}`)
  }

  for (const fn of ['get', 'createWriteStream', 'delete']) {
    if (typeof store[fn] !== 'function') {
      throw new TypeError(`${name} needs to have a \`${fn}()\` function`)
    }
  }
}
/**
 * @param {unknown} methods
 * @returns {asserts methods is import('../../types/cache-interceptor.d.ts').default.CacheMethods[]}
 */
function assertCacheMethods (methods, name = 'CacheMethods') {
  if (!Array.isArray(methods)) {
    throw new TypeError(`expected type of ${name} needs to be an array, got ${methods === null ? 'null' : typeof methods}`)
  }

  if (methods.length === 0) {
    throw new TypeError(`${name} needs to have at least one method`)
  }

  for (const method of methods) {
    if (!arrayIncludes(safeHTTPMethods, method)) {
      throw new TypeError(`element of ${name}-array needs to be one of following values: ${safeHTTPMethods.join(', ')}, got ${method}`)
    }
  }
}

/**
 * Creates a string key for request deduplication purposes.
 * This key is used to identify in-flight requests that can be shared.
 * @param {import('../../types/cache-interceptor.d.ts').default.CacheKey} cacheKey
 * @param {Set<string>} [excludeHeaders] Set of lowercase header names to exclude from the key
 * @returns {string}
 */
function makeDeduplicationKey (cacheKey, excludeHeaders) {
  // Use JSON.stringify to produce a collision-resistant key.
  // Previous format used `:` and `=` delimiters without escaping, which
  // allowed different header sets to produce identical keys (e.g.
  // {a:"x:b=y"} vs {a:"x", b:"y"}). See: https://github.com/nodejs/undici/issues/5012
  const headers = {}

  if (cacheKey.headers) {
    const sortedHeaders = Object.keys(cacheKey.headers).sort()
    for (const header of sortedHeaders) {
      // Skip excluded headers
      if (excludeHeaders?.has(header.toLowerCase())) {
        continue
      }
      headers[header] = cacheKey.headers[header]
    }
  }

  return JSON.stringify([cacheKey.origin, cacheKey.method, cacheKey.path, headers])
}

module.exports = {
  makeCacheKey,
  normalizeHeaders,
  assertCacheKey,
  assertCacheValue,
  parseCacheControlHeader,
  hasInvalidCacheControlDirective,
  parseVaryHeader,
  hasVaryStar,
  isInvalidOrWildcardVaryHeader,
  isEtagUsable,
  assertCacheMethods,
  assertCacheStore,
  makeDeduplicationKey
}
