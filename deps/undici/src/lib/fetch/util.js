'use strict'

const { redirectStatus } = require('./constants')
const { performance } = require('perf_hooks')
const { isBlobLike, toUSVString, ReadableStreamFrom } = require('../core/util')
const assert = require('assert')

let File

// https://fetch.spec.whatwg.org/#block-bad-port
const badPorts = [
  '1', '7', '9', '11', '13', '15', '17', '19', '20', '21', '22', '23', '25', '37', '42', '43', '53', '69', '77', '79',
  '87', '95', '101', '102', '103', '104', '109', '110', '111', '113', '115', '117', '119', '123', '135', '137',
  '139', '143', '161', '179', '389', '427', '465', '512', '513', '514', '515', '526', '530', '531', '532',
  '540', '548', '554', '556', '563', '587', '601', '636', '989', '990', '993', '995', '1719', '1720', '1723',
  '2049', '3659', '4045', '5060', '5061', '6000', '6566', '6665', '6666', '6667', '6668', '6669', '6697',
  '10080'
]

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

function isFileLike (object) {
  if (!File) {
    File = require('./file').File
  }
  return object instanceof File || (
    object &&
    (typeof object.stream === 'function' ||
     typeof object.arrayBuffer === 'function') &&
    /^(File)$/.test(object[Symbol.toStringTag])
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

  for (const char of potentialValue) {
    if (!isValidHTTPToken(char)) {
      return false
    }
  }

  return true
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
  // TODO:  https://w3c.github.io/webappsec-referrer-policy/#parse-referrer-policy-from-header
  const policy = ''

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
  // TODO
  return 'no-referrer'
}

function matchRequestIntegrity (request, bytes) {
  return false
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

// https://webidl.spec.whatwg.org/#dfn-iterator-prototype-object
function makeIterator (iterator, name) {
  const i = {
    next () {
      if (Object.getPrototypeOf(this) !== i) {
        throw new TypeError(
          `'next' called on an object that does not implement interface ${name} Iterator.`
        )
      }

      return iterator.next()
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
  matchRequestIntegrity,
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
  isFileLike,
  isValidReasonPhrase,
  sameOrigin,
  normalizeMethod,
  serializeJavascriptValueToJSONString,
  makeIterator,
  isValidHeaderName,
  isValidHeaderValue,
  hasOwn
}
