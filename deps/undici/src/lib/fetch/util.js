'use strict'

const { redirectStatus, badPorts, referrerPolicy: referrerPolicyTokens } = require('./constants')
const { performance } = require('perf_hooks')
const { isBlobLike, toUSVString, ReadableStreamFrom } = require('../core/util')
const assert = require('assert')
const { isUint8Array } = require('util/types')

// https://nodejs.org/api/crypto.html#determining-if-crypto-support-is-unavailable
/** @type {import('crypto')|undefined} */
let crypto

try {
  crypto = require('crypto')
} catch {

}

function responseURL (response) {
  // https://fetch.spec.whatwg.org/#responses
  // A response has an associated URL. It is a pointer to the last URL
  // in response’s URL list and null if response’s URL list is empty.
  const urlList = response.urlList
  const length = urlList.length
  return length === 0 ? null : urlList[length - 1].toString()
}

// https://fetch.spec.whatwg.org/#concept-response-location-url
function responseLocationURL (response, requestFragment) {
  // 1. If response’s status is not a redirect status, then return null.
  if (!redirectStatus.includes(response.status)) {
    return null
  }

  // 2. Let location be the result of extracting header list values given
  // `Location` and response’s header list.
  let location = response.headersList.get('location')

  // 3. If location is a value, then set location to the result of parsing
  // location with response’s URL.
  location = location ? new URL(location, responseURL(response)) : null

  // 4. If location is a URL whose fragment is null, then set location’s
  // fragment to requestFragment.
  if (location && !location.hash) {
    location.hash = requestFragment
  }

  // 5. Return location.
  return location
}

/** @returns {URL} */
function requestCurrentURL (request) {
  return request.urlList[request.urlList.length - 1]
}

function requestBadPort (request) {
  // 1. Let url be request’s current URL.
  const url = requestCurrentURL(request)

  // 2. If url’s scheme is an HTTP(S) scheme and url’s port is a bad port,
  // then return blocked.
  if (/^https?:/.test(url.protocol) && badPorts.includes(url.port)) {
    return 'blocked'
  }

  // 3. Return allowed.
  return 'allowed'
}

function isErrorLike (object) {
  return object instanceof Error || (
    object?.constructor?.name === 'Error' ||
    object?.constructor?.name === 'DOMException'
  )
}

// Check whether |statusText| is a ByteString and
// matches the Reason-Phrase token production.
// RFC 2616: https://tools.ietf.org/html/rfc2616
// RFC 7230: https://tools.ietf.org/html/rfc7230
// "reason-phrase = *( HTAB / SP / VCHAR / obs-text )"
// https://github.com/chromium/chromium/blob/94.0.4604.1/third_party/blink/renderer/core/fetch/response.cc#L116
function isValidReasonPhrase (statusText) {
  for (let i = 0; i < statusText.length; ++i) {
    const c = statusText.charCodeAt(i)
    if (
      !(
        (
          c === 0x09 || // HTAB
          (c >= 0x20 && c <= 0x7e) || // SP / VCHAR
          (c >= 0x80 && c <= 0xff)
        ) // obs-text
      )
    ) {
      return false
    }
  }
  return true
}

function isTokenChar (c) {
  return !(
    c >= 0x7f ||
    c <= 0x20 ||
    c === '(' ||
    c === ')' ||
    c === '<' ||
    c === '>' ||
    c === '@' ||
    c === ',' ||
    c === ';' ||
    c === ':' ||
    c === '\\' ||
    c === '"' ||
    c === '/' ||
    c === '[' ||
    c === ']' ||
    c === '?' ||
    c === '=' ||
    c === '{' ||
    c === '}'
  )
}

// See RFC 7230, Section 3.2.6.
// https://github.com/chromium/chromium/blob/d7da0240cae77824d1eda25745c4022757499131/third_party/blink/renderer/platform/network/http_parsers.cc#L321
function isValidHTTPToken (characters) {
  if (!characters || typeof characters !== 'string') {
    return false
  }
  for (let i = 0; i < characters.length; ++i) {
    const c = characters.charCodeAt(i)
    if (c > 0x7f || !isTokenChar(c)) {
      return false
    }
  }
  return true
}

// https://fetch.spec.whatwg.org/#header-name
// https://github.com/chromium/chromium/blob/b3d37e6f94f87d59e44662d6078f6a12de845d17/net/http/http_util.cc#L342
function isValidHeaderName (potentialValue) {
  if (potentialValue.length === 0) {
    return false
  }

  return isValidHTTPToken(potentialValue)
}

/**
 * @see https://fetch.spec.whatwg.org/#header-value
 * @param {string} potentialValue
 */
function isValidHeaderValue (potentialValue) {
  // - Has no leading or trailing HTTP tab or space bytes.
  // - Contains no 0x00 (NUL) or HTTP newline bytes.
  if (
    potentialValue.startsWith('\t') ||
    potentialValue.startsWith(' ') ||
    potentialValue.endsWith('\t') ||
    potentialValue.endsWith(' ')
  ) {
    return false
  }

  if (
    potentialValue.includes('\0') ||
    potentialValue.includes('\r') ||
    potentialValue.includes('\n')
  ) {
    return false
  }

  return true
}

// https://w3c.github.io/webappsec-referrer-policy/#set-requests-referrer-policy-on-redirect
function setRequestReferrerPolicyOnRedirect (request, actualResponse) {
  //  Given a request request and a response actualResponse, this algorithm
  //  updates request’s referrer policy according to the Referrer-Policy
  //  header (if any) in actualResponse.

  // 1. Let policy be the result of executing § 8.1 Parse a referrer policy
  // from a Referrer-Policy header on actualResponse.

  // 8.1 Parse a referrer policy from a Referrer-Policy header
  // 1. Let policy-tokens be the result of extracting header list values given `Referrer-Policy` and response’s header list.
  const { headersList } = actualResponse
  // 2. Let policy be the empty string.
  // 3. For each token in policy-tokens, if token is a referrer policy and token is not the empty string, then set policy to token.
  // 4. Return policy.
  const policyHeader = (headersList.get('referrer-policy') ?? '').split(',')

  // Note: As the referrer-policy can contain multiple policies
  // separated by comma, we need to loop through all of them
  // and pick the first valid one.
  // Ref: https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Referrer-Policy#specify_a_fallback_policy
  let policy = ''
  if (policyHeader.length > 0) {
    // The right-most policy takes precedence.
    // The left-most policy is the fallback.
    for (let i = policyHeader.length; i !== 0; i--) {
      const token = policyHeader[i - 1].trim()
      if (referrerPolicyTokens.includes(token)) {
        policy = token
        break
      }
    }
  }

  // 2. If policy is not the empty string, then set request’s referrer policy to policy.
  if (policy !== '') {
    request.referrerPolicy = policy
  }
}

// https://fetch.spec.whatwg.org/#cross-origin-resource-policy-check
function crossOriginResourcePolicyCheck () {
  // TODO
  return 'allowed'
}

// https://fetch.spec.whatwg.org/#concept-cors-check
function corsCheck () {
  // TODO
  return 'success'
}

// https://fetch.spec.whatwg.org/#concept-tao-check
function TAOCheck () {
  // TODO
  return 'success'
}

function appendFetchMetadata (httpRequest) {
  //  https://w3c.github.io/webappsec-fetch-metadata/#sec-fetch-dest-header
  //  TODO

  //  https://w3c.github.io/webappsec-fetch-metadata/#sec-fetch-mode-header

  //  1. Assert: r’s url is a potentially trustworthy URL.
  //  TODO

  //  2. Let header be a Structured Header whose value is a token.
  let header = null

  //  3. Set header’s value to r’s mode.
  header = httpRequest.mode

  //  4. Set a structured field value `Sec-Fetch-Mode`/header in r’s header list.
  httpRequest.headersList.set('sec-fetch-mode', header)

  //  https://w3c.github.io/webappsec-fetch-metadata/#sec-fetch-site-header
  //  TODO

  //  https://w3c.github.io/webappsec-fetch-metadata/#sec-fetch-user-header
  //  TODO
}

// https://fetch.spec.whatwg.org/#append-a-request-origin-header
function appendRequestOriginHeader (request) {
  // 1. Let serializedOrigin be the result of byte-serializing a request origin with request.
  let serializedOrigin = request.origin

  // 2. If request’s response tainting is "cors" or request’s mode is "websocket", then append (`Origin`, serializedOrigin) to request’s header list.
  if (request.responseTainting === 'cors' || request.mode === 'websocket') {
    if (serializedOrigin) {
      request.headersList.append('Origin', serializedOrigin)
    }

  // 3. Otherwise, if request’s method is neither `GET` nor `HEAD`, then:
  } else if (request.method !== 'GET' && request.method !== 'HEAD') {
    // 1. Switch on request’s referrer policy:
    switch (request.referrerPolicy) {
      case 'no-referrer':
        // Set serializedOrigin to `null`.
        serializedOrigin = null
        break
      case 'no-referrer-when-downgrade':
      case 'strict-origin':
      case 'strict-origin-when-cross-origin':
        // If request’s origin is a tuple origin, its scheme is "https", and request’s current URL’s scheme is not "https", then set serializedOrigin to `null`.
        if (/^https:/.test(request.origin) && !/^https:/.test(requestCurrentURL(request))) {
          serializedOrigin = null
        }
        break
      case 'same-origin':
        // If request’s origin is not same origin with request’s current URL’s origin, then set serializedOrigin to `null`.
        if (!sameOrigin(request, requestCurrentURL(request))) {
          serializedOrigin = null
        }
        break
      default:
        // Do nothing.
    }

    if (serializedOrigin) {
      // 2. Append (`Origin`, serializedOrigin) to request’s header list.
      request.headersList.append('Origin', serializedOrigin)
    }
  }
}

function coarsenedSharedCurrentTime (crossOriginIsolatedCapability) {
  // TODO
  return performance.now()
}

// https://fetch.spec.whatwg.org/#create-an-opaque-timing-info
function createOpaqueTimingInfo (timingInfo) {
  return {
    startTime: timingInfo.startTime ?? 0,
    redirectStartTime: 0,
    redirectEndTime: 0,
    postRedirectStartTime: timingInfo.startTime ?? 0,
    finalServiceWorkerStartTime: 0,
    finalNetworkResponseStartTime: 0,
    finalNetworkRequestStartTime: 0,
    endTime: 0,
    encodedBodySize: 0,
    decodedBodySize: 0,
    finalConnectionTimingInfo: null
  }
}

// https://html.spec.whatwg.org/multipage/origin.html#policy-container
function makePolicyContainer () {
  // TODO
  return {}
}

// https://html.spec.whatwg.org/multipage/origin.html#clone-a-policy-container
function clonePolicyContainer () {
  // TODO
  return {}
}

// https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
function determineRequestsReferrer (request) {
  // 1. Let policy be request's referrer policy.
  const policy = request.referrerPolicy

  // Return no-referrer when empty or policy says so
  if (policy == null || policy === '' || policy === 'no-referrer') {
    return 'no-referrer'
  }

  // 2. Let environment be the request client
  const environment = request.client
  let referrerSource = null

  /**
   * 3, Switch on request’s referrer:
    "client"
      If environment’s global object is a Window object, then
        Let document be the associated Document of environment’s global object.
        If document’s origin is an opaque origin, return no referrer.
        While document is an iframe srcdoc document,
        let document be document’s browsing context’s browsing context container’s node document.
        Let referrerSource be document’s URL.

      Otherwise, let referrerSource be environment’s creation URL.

    a URL
    Let referrerSource be request’s referrer.
   */
  if (request.referrer === 'client') {
    // Not defined in Node but part of the spec
    if (request.client?.globalObject?.constructor?.name === 'Window' ) { // eslint-disable-line
      const origin = environment.globalObject.self?.origin ?? environment.globalObject.location?.origin

      // If document’s origin is an opaque origin, return no referrer.
      if (origin == null || origin === 'null') return 'no-referrer'

      // Let referrerSource be document’s URL.
      referrerSource = new URL(environment.globalObject.location.href)
    } else {
      // 3(a)(II) If environment's global object is not Window,
      // Let referrerSource be environments creationURL
      if (environment?.globalObject?.location == null) {
        return 'no-referrer'
      }

      referrerSource = new URL(environment.globalObject.location.href)
    }
  } else if (request.referrer instanceof URL) {
    // 3(b) If requests's referrer is a URL instance, then make
    // referrerSource be requests's referrer.
    referrerSource = request.referrer
  } else {
    // If referrerSource neither client nor instance of URL
    // then return "no-referrer".
    return 'no-referrer'
  }

  const urlProtocol = referrerSource.protocol

  // If url's scheme is a local scheme (i.e. one of "about", "data", "javascript", "file")
  // then return "no-referrer".
  if (
    urlProtocol === 'about:' || urlProtocol === 'data:' ||
    urlProtocol === 'blob:'
  ) {
    return 'no-referrer'
  }

  let temp
  let referrerOrigin
  // 4. Let requests's referrerURL be the result of stripping referrer
  // source for use as referrer (using util function, without origin only)
  const referrerUrl = (temp = stripURLForReferrer(referrerSource)).length > 4096
  // 5. Let referrerOrigin be the result of stripping referrer
  // source for use as referrer (using util function, with originOnly true)
    ? (referrerOrigin = stripURLForReferrer(referrerSource, true))
  // 6. If result of seralizing referrerUrl is a string whose length is greater than
  // 4096, then set referrerURL to referrerOrigin
    : temp
  const areSameOrigin = sameOrigin(request, referrerUrl)
  const isNonPotentiallyTrustWorthy = isURLPotentiallyTrustworthy(referrerUrl) &&
    !isURLPotentiallyTrustworthy(request.url)

  // NOTE: How to treat step 7?
  // 8. Execute the switch statements corresponding to the value of policy:
  switch (policy) {
    case 'origin': return referrerOrigin != null ? referrerOrigin : stripURLForReferrer(referrerSource, true)
    case 'unsafe-url': return referrerUrl
    case 'same-origin':
      return areSameOrigin ? referrerOrigin : 'no-referrer'
    case 'origin-when-cross-origin':
      return areSameOrigin ? referrerUrl : referrerOrigin
    case 'strict-origin-when-cross-origin':
      /**
         * 1. If the origin of referrerURL and the origin of request’s current URL are the same,
         * then return referrerURL.
         * 2. If referrerURL is a potentially trustworthy URL and request’s current URL is not a
         * potentially trustworthy URL, then return no referrer.
         * 3. Return referrerOrigin
      */
      if (areSameOrigin) return referrerOrigin
      // else return isNonPotentiallyTrustWorthy ? 'no-referrer' : referrerOrigin
    case 'strict-origin': // eslint-disable-line
      /**
         * 1. If referrerURL is a potentially trustworthy URL and
         * request’s current URL is not a potentially trustworthy URL,
         * then return no referrer.
         * 2. Return referrerOrigin
        */
    case 'no-referrer-when-downgrade': // eslint-disable-line
      /**
       * 1. If referrerURL is a potentially trustworthy URL and
       * request’s current URL is not a potentially trustworthy URL,
       * then return no referrer.
       * 2. Return referrerOrigin
      */

    default: // eslint-disable-line
      return isNonPotentiallyTrustWorthy ? 'no-referrer' : referrerOrigin
  }

  function stripURLForReferrer (url, originOnly = false) {
    const urlObject = new URL(url.href)
    urlObject.username = ''
    urlObject.password = ''
    urlObject.hash = ''

    return originOnly ? urlObject.origin : urlObject.href
  }
}

function isURLPotentiallyTrustworthy (url) {
  if (!(url instanceof URL)) {
    return false
  }

  // If child of about, return true
  if (url.href === 'about:blank' || url.href === 'about:srcdoc') {
    return true
  }

  // If scheme is data, return true
  if (url.protocol === 'data:') return true

  // If file, return true
  if (url.protocol === 'file:') return true

  return isOriginPotentiallyTrustworthy(url.origin)

  function isOriginPotentiallyTrustworthy (origin) {
    // If origin is explicitly null, return false
    if (origin == null || origin === 'null') return false

    const originAsURL = new URL(origin)

    // If secure, return true
    if (originAsURL.protocol === 'https:' || originAsURL.protocol === 'wss:') {
      return true
    }

    // If localhost or variants, return true
    if (/^127(?:\.[0-9]+){0,2}\.[0-9]+$|^\[(?:0*:)*?:?0*1\]$/.test(originAsURL.hostname) ||
     (originAsURL.hostname === 'localhost' || originAsURL.hostname.includes('localhost.')) ||
     (originAsURL.hostname.endsWith('.localhost'))) {
      return true
    }

    // If any other, return false
    return false
  }
}

/**
 * @see https://w3c.github.io/webappsec-subresource-integrity/#does-response-match-metadatalist
 * @param {Uint8Array} bytes
 * @param {string} metadataList
 */
function bytesMatch (bytes, metadataList) {
  // If node is not built with OpenSSL support, we cannot check
  // a request's integrity, so allow it by default (the spec will
  // allow requests if an invalid hash is given, as precedence).
  /* istanbul ignore if: only if node is built with --without-ssl */
  if (crypto === undefined) {
    return true
  }

  // 1. Let parsedMetadata be the result of parsing metadataList.
  const parsedMetadata = parseMetadata(metadataList)

  // 2. If parsedMetadata is no metadata, return true.
  if (parsedMetadata === 'no metadata') {
    return true
  }

  // 3. If parsedMetadata is the empty set, return true.
  if (parsedMetadata.length === 0) {
    return true
  }

  // 4. Let metadata be the result of getting the strongest
  //    metadata from parsedMetadata.
  const list = parsedMetadata.sort((c, d) => d.algo.localeCompare(c.algo))
  // get the strongest algorithm
  const strongest = list[0].algo
  // get all entries that use the strongest algorithm; ignore weaker
  const metadata = list.filter((item) => item.algo === strongest)

  // 5. For each item in metadata:
  for (const item of metadata) {
    // 1. Let algorithm be the alg component of item.
    const algorithm = item.algo

    // 2. Let expectedValue be the val component of item.
    const expectedValue = item.hash

    // 3. Let actualValue be the result of applying algorithm to bytes.
    const actualValue = crypto.createHash(algorithm).update(bytes).digest('base64')

    // 4. If actualValue is a case-sensitive match for expectedValue,
    //    return true.
    if (actualValue === expectedValue) {
      return true
    }
  }

  // 6. Return false.
  return false
}

// https://w3c.github.io/webappsec-subresource-integrity/#grammardef-hash-with-options
// https://www.w3.org/TR/CSP2/#source-list-syntax
// https://www.rfc-editor.org/rfc/rfc5234#appendix-B.1
const parseHashWithOptions = /((?<algo>sha256|sha384|sha512)-(?<hash>[A-z0-9+/]{1}.*={0,2}))( +[\x21-\x7e]?)?/i

/**
 * @see https://w3c.github.io/webappsec-subresource-integrity/#parse-metadata
 * @param {string} metadata
 */
function parseMetadata (metadata) {
  // 1. Let result be the empty set.
  /** @type {{ algo: string, hash: string }[]} */
  const result = []

  // 2. Let empty be equal to true.
  let empty = true

  const supportedHashes = crypto.getHashes()

  // 3. For each token returned by splitting metadata on spaces:
  for (const token of metadata.split(' ')) {
    // 1. Set empty to false.
    empty = false

    // 2. Parse token as a hash-with-options.
    const parsedToken = parseHashWithOptions.exec(token)

    // 3. If token does not parse, continue to the next token.
    if (parsedToken === null || parsedToken.groups === undefined) {
      // Note: Chromium blocks the request at this point, but Firefox
      // gives a warning that an invalid integrity was given. The
      // correct behavior is to ignore these, and subsequently not
      // check the integrity of the resource.
      continue
    }

    // 4. Let algorithm be the hash-algo component of token.
    const algorithm = parsedToken.groups.algo

    // 5. If algorithm is a hash function recognized by the user
    //    agent, add the parsed token to result.
    if (supportedHashes.includes(algorithm.toLowerCase())) {
      result.push(parsedToken.groups)
    }
  }

  // 4. Return no metadata if empty is true, otherwise return result.
  if (empty === true) {
    return 'no metadata'
  }

  return result
}

// https://w3c.github.io/webappsec-upgrade-insecure-requests/#upgrade-request
function tryUpgradeRequestToAPotentiallyTrustworthyURL (request) {
  // TODO
}

/**
 * @link {https://html.spec.whatwg.org/multipage/origin.html#same-origin}
 * @param {URL} A
 * @param {URL} B
 */
function sameOrigin (A, B) {
  // 1. If A and B are the same opaque origin, then return true.
  // "opaque origin" is an internal value we cannot access, ignore.

  // 2. If A and B are both tuple origins and their schemes,
  //    hosts, and port are identical, then return true.
  if (A.protocol === B.protocol && A.hostname === B.hostname && A.port === B.port) {
    return true
  }

  // 3. Return false.
  return false
}

function createDeferredPromise () {
  let res
  let rej
  const promise = new Promise((resolve, reject) => {
    res = resolve
    rej = reject
  })

  return { promise, resolve: res, reject: rej }
}

function isAborted (fetchParams) {
  return fetchParams.controller.state === 'aborted'
}

function isCancelled (fetchParams) {
  return fetchParams.controller.state === 'aborted' ||
    fetchParams.controller.state === 'terminated'
}

// https://fetch.spec.whatwg.org/#concept-method-normalize
function normalizeMethod (method) {
  return /^(DELETE|GET|HEAD|OPTIONS|POST|PUT)$/i.test(method)
    ? method.toUpperCase()
    : method
}

// https://infra.spec.whatwg.org/#serialize-a-javascript-value-to-a-json-string
function serializeJavascriptValueToJSONString (value) {
  // 1. Let result be ? Call(%JSON.stringify%, undefined, « value »).
  const result = JSON.stringify(value)

  // 2. If result is undefined, then throw a TypeError.
  if (result === undefined) {
    throw new TypeError('Value is not JSON serializable')
  }

  // 3. Assert: result is a string.
  assert(typeof result === 'string')

  // 4. Return result.
  return result
}

// https://tc39.es/ecma262/#sec-%25iteratorprototype%25-object
const esIteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()))

/**
 * @see https://webidl.spec.whatwg.org/#dfn-iterator-prototype-object
 * @param {() => unknown[]} iterator
 * @param {string} name name of the instance
 * @param {'key'|'value'|'key+value'} kind
 */
function makeIterator (iterator, name, kind) {
  const object = {
    index: 0,
    kind,
    target: iterator
  }

  const i = {
    next () {
      // 1. Let interface be the interface for which the iterator prototype object exists.

      // 2. Let thisValue be the this value.

      // 3. Let object be ? ToObject(thisValue).

      // 4. If object is a platform object, then perform a security
      //    check, passing:

      // 5. If object is not a default iterator object for interface,
      //    then throw a TypeError.
      if (Object.getPrototypeOf(this) !== i) {
        throw new TypeError(
          `'next' called on an object that does not implement interface ${name} Iterator.`
        )
      }

      // 6. Let index be object’s index.
      // 7. Let kind be object’s kind.
      // 8. Let values be object’s target's value pairs to iterate over.
      const { index, kind, target } = object
      const values = target()

      // 9. Let len be the length of values.
      const len = values.length

      // 10. If index is greater than or equal to len, then return
      //     CreateIterResultObject(undefined, true).
      if (index >= len) {
        return { value: undefined, done: true }
      }

      // 11. Let pair be the entry in values at index index.
      const pair = values[index]

      // 12. Set object’s index to index + 1.
      object.index = index + 1

      // 13. Return the iterator result for pair and kind.
      return iteratorResult(pair, kind)
    },
    // The class string of an iterator prototype object for a given interface is the
    // result of concatenating the identifier of the interface and the string " Iterator".
    [Symbol.toStringTag]: `${name} Iterator`
  }

  // The [[Prototype]] internal slot of an iterator prototype object must be %IteratorPrototype%.
  Object.setPrototypeOf(i, esIteratorPrototype)
  // esIteratorPrototype needs to be the prototype of i
  // which is the prototype of an empty object. Yes, it's confusing.
  return Object.setPrototypeOf({}, i)
}

// https://webidl.spec.whatwg.org/#iterator-result
function iteratorResult (pair, kind) {
  let result

  // 1. Let result be a value determined by the value of kind:
  switch (kind) {
    case 'key': {
      // 1. Let idlKey be pair’s key.
      // 2. Let key be the result of converting idlKey to an
      //    ECMAScript value.
      // 3. result is key.
      result = pair[0]
      break
    }
    case 'value': {
      // 1. Let idlValue be pair’s value.
      // 2. Let value be the result of converting idlValue to
      //    an ECMAScript value.
      // 3. result is value.
      result = pair[1]
      break
    }
    case 'key+value': {
      // 1. Let idlKey be pair’s key.
      // 2. Let idlValue be pair’s value.
      // 3. Let key be the result of converting idlKey to an
      //    ECMAScript value.
      // 4. Let value be the result of converting idlValue to
      //    an ECMAScript value.
      // 5. Let array be ! ArrayCreate(2).
      // 6. Call ! CreateDataProperty(array, "0", key).
      // 7. Call ! CreateDataProperty(array, "1", value).
      // 8. result is array.
      result = pair
      break
    }
  }

  // 2. Return CreateIterResultObject(result, false).
  return { value: result, done: false }
}

/**
 * @see https://fetch.spec.whatwg.org/#body-fully-read
 */
async function fullyReadBody (body, processBody, processBodyError) {
  // 1. If taskDestination is null, then set taskDestination to
  //    the result of starting a new parallel queue.

  // 2. Let promise be the result of fully reading body as promise
  //    given body.
  try {
    /** @type {Uint8Array[]} */
    const chunks = []
    let length = 0

    const reader = body.stream.getReader()

    while (true) {
      const { done, value } = await reader.read()

      if (done === true) {
        break
      }

      // read-loop chunk steps
      assert(isUint8Array(value))

      chunks.push(value)
      length += value.byteLength
    }

    // 3. Let fulfilledSteps given a byte sequence bytes be to queue
    //    a fetch task to run processBody given bytes, with
    //    taskDestination.
    const fulfilledSteps = (bytes) => queueMicrotask(() => {
      processBody(bytes)
    })

    fulfilledSteps(Buffer.concat(chunks, length))
  } catch (err) {
    // 4. Let rejectedSteps be to queue a fetch task to run
    //    processBodyError, with taskDestination.
    queueMicrotask(() => processBodyError(err))
  }

  // 5. React to promise with fulfilledSteps and rejectedSteps.
}

/** @type {ReadableStream} */
let ReadableStream = globalThis.ReadableStream

function isReadableStreamLike (stream) {
  if (!ReadableStream) {
    ReadableStream = require('stream/web').ReadableStream
  }

  return stream instanceof ReadableStream || (
    stream[Symbol.toStringTag] === 'ReadableStream' &&
    typeof stream.tee === 'function'
  )
}

const MAXIMUM_ARGUMENT_LENGTH = 65535

/**
 * @see https://infra.spec.whatwg.org/#isomorphic-decode
 * @param {number[]|Uint8Array} input
 */
function isomorphicDecode (input) {
  // 1. To isomorphic decode a byte sequence input, return a string whose code point
  //    length is equal to input’s length and whose code points have the same values
  //    as the values of input’s bytes, in the same order.

  if (input.length < MAXIMUM_ARGUMENT_LENGTH) {
    return String.fromCharCode(...input)
  }

  return input.reduce((previous, current) => previous + String.fromCharCode(current), '')
}

/**
 * @param {ReadableStreamController<Uint8Array>} controller
 */
function readableStreamClose (controller) {
  try {
    controller.close()
  } catch (err) {
    // TODO: add comment explaining why this error occurs.
    if (!err.message.includes('Controller is already closed')) {
      throw err
    }
  }
}

/**
 * @see https://infra.spec.whatwg.org/#isomorphic-encode
 * @param {string} input
 */
function isomorphicEncode (input) {
  // 1. Assert: input contains no code points greater than U+00FF.
  for (let i = 0; i < input.length; i++) {
    assert(input.charCodeAt(i) <= 0xFF)
  }

  // 2. Return a byte sequence whose length is equal to input’s code
  //    point length and whose bytes have the same values as the
  //    values of input’s code points, in the same order
  return input
}

/**
 * Fetch supports node >= 16.8.0, but Object.hasOwn was added in v16.9.0.
 */
const hasOwn = Object.hasOwn || ((dict, key) => Object.prototype.hasOwnProperty.call(dict, key))

module.exports = {
  isAborted,
  isCancelled,
  createDeferredPromise,
  ReadableStreamFrom,
  toUSVString,
  tryUpgradeRequestToAPotentiallyTrustworthyURL,
  coarsenedSharedCurrentTime,
  determineRequestsReferrer,
  makePolicyContainer,
  clonePolicyContainer,
  appendFetchMetadata,
  appendRequestOriginHeader,
  TAOCheck,
  corsCheck,
  crossOriginResourcePolicyCheck,
  createOpaqueTimingInfo,
  setRequestReferrerPolicyOnRedirect,
  isValidHTTPToken,
  requestBadPort,
  requestCurrentURL,
  responseURL,
  responseLocationURL,
  isBlobLike,
  isURLPotentiallyTrustworthy,
  isValidReasonPhrase,
  sameOrigin,
  normalizeMethod,
  serializeJavascriptValueToJSONString,
  makeIterator,
  isValidHeaderName,
  isValidHeaderValue,
  hasOwn,
  isErrorLike,
  fullyReadBody,
  bytesMatch,
  isReadableStreamLike,
  readableStreamClose,
  isomorphicEncode,
  isomorphicDecode
}
