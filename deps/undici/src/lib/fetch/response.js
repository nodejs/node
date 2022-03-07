'use strict'

const { Headers, HeadersList, fill } = require('./headers')
const { extractBody, cloneBody, mixinBody } = require('./body')
const util = require('../core/util')
const { kEnumerableProperty } = util
const { responseURL, isValidReasonPhrase, toUSVString } = require('./util')
const {
  redirectStatus,
  nullBodyStatus,
  forbiddenResponseHeaderNames
} = require('./constants')
const { kState, kHeaders, kGuard, kRealm } = require('./symbols')
const { kHeadersList } = require('../core/symbols')
const assert = require('assert')

// https://fetch.spec.whatwg.org/#response-class
class Response {
  // Creates network error Response.
  static error () {
    // TODO
    const relevantRealm = { settingsObject: {} }

    // The static error() method steps are to return the result of creating a
    // Response object, given a new network error, "immutable", and this’s
    // relevant Realm.
    const responseObject = new Response()
    responseObject[kState] = makeNetworkError()
    responseObject[kRealm] = relevantRealm
    responseObject[kHeaders][kHeadersList] = responseObject[kState].headersList
    responseObject[kHeaders][kGuard] = 'immutable'
    responseObject[kHeaders][kRealm] = relevantRealm
    return responseObject
  }

  // Creates a redirect Response that redirects to url with status status.
  static redirect (...args) {
    const relevantRealm = { settingsObject: {} }

    if (args.length < 1) {
      throw new TypeError(
        `Failed to execute 'redirect' on 'Response': 1 argument required, but only ${args.length} present.`
      )
    }

    const status = args.length >= 2 ? args[1] : 302
    const url = toUSVString(args[0])

    // 1. Let parsedURL be the result of parsing url with current settings
    // object’s API base URL.
    // 2. If parsedURL is failure, then throw a TypeError.
    // TODO: base-URL?
    let parsedURL
    try {
      parsedURL = new URL(url)
    } catch (err) {
      throw Object.assign(new TypeError('Failed to parse URL from ' + url), {
        cause: err
      })
    }

    // 3. If status is not a redirect status, then throw a RangeError.
    if (!redirectStatus.includes(status)) {
      throw new RangeError('Invalid status code')
    }

    // 4. Let responseObject be the result of creating a Response object,
    // given a new response, "immutable", and this’s relevant Realm.
    const responseObject = new Response()
    responseObject[kRealm] = relevantRealm
    responseObject[kHeaders][kGuard] = 'immutable'
    responseObject[kHeaders][kRealm] = relevantRealm

    // 5. Set responseObject’s response’s status to status.
    responseObject[kState].status = status

    // 6. Let value be parsedURL, serialized and isomorphic encoded.
    // TODO: isomorphic encoded?
    const value = parsedURL.toString()

    // 7. Append `Location`/value to responseObject’s response’s header list.
    responseObject[kState].headersList.push('location', value)

    // 8. Return responseObject.
    return responseObject
  }

  // https://fetch.spec.whatwg.org/#dom-response
  constructor (...args) {
    if (
      args.length >= 1 &&
      typeof args[1] !== 'object' &&
      args[1] !== undefined
    ) {
      throw new TypeError(
        "Failed to construct 'Request': cannot convert to dictionary."
      )
    }

    const body = args.length >= 1 ? args[0] : null
    const init = args.length >= 2 ? args[1] ?? {} : {}

    // TODO
    this[kRealm] = { settingsObject: {} }

    // 1. If init["status"] is not in the range 200 to 599, inclusive, then
    // throw a RangeError.
    if ('status' in init && init.status !== undefined) {
      if (!Number.isFinite(init.status)) {
        throw new TypeError()
      }

      if (init.status < 200 || init.status > 599) {
        throw new RangeError(
          `Failed to construct 'Response': The status provided (${init.status}) is outside the range [200, 599].`
        )
      }
    }

    if ('statusText' in init && init.statusText !== undefined) {
      // 2. If init["statusText"] does not match the reason-phrase token
      // production, then throw a TypeError.
      // See, https://datatracker.ietf.org/doc/html/rfc7230#section-3.1.2:
      //   reason-phrase  = *( HTAB / SP / VCHAR / obs-text )
      if (!isValidReasonPhrase(String(init.statusText))) {
        throw new TypeError('Invalid statusText')
      }
    }

    // 3. Set this’s response to a new response.
    this[kState] = makeResponse({})

    // 4. Set this’s headers to a new Headers object with this’s relevant
    // Realm, whose header list is this’s response’s header list and guard
    // is "response".
    this[kHeaders] = new Headers()
    this[kHeaders][kGuard] = 'response'
    this[kHeaders][kHeadersList] = this[kState].headersList
    this[kHeaders][kRealm] = this[kRealm]

    // 5. Set this’s response’s status to init["status"].
    if ('status' in init && init.status !== undefined) {
      this[kState].status = init.status
    }

    // 6. Set this’s response’s status message to init["statusText"].
    if ('statusText' in init && init.statusText !== undefined) {
      this[kState].statusText = String(init.statusText)
    }

    // 7. If init["headers"] exists, then fill this’s headers with init["headers"].
    if ('headers' in init) {
      fill(this[kState].headersList, init.headers)
    }

    // 8. If body is non-null, then:
    if (body != null) {
      // 1. If init["status"] is a null body status, then throw a TypeError.
      if (nullBodyStatus.includes(init.status)) {
        throw new TypeError('Response with null body status cannot have body')
      }

      // 2. Let Content-Type be null.
      // 3. Set this’s response’s body and Content-Type to the result of
      // extracting body.
      const [extractedBody, contentType] = extractBody(body)
      this[kState].body = extractedBody

      // 4. If Content-Type is non-null and this’s response’s header list does
      // not contain `Content-Type`, then append `Content-Type`/Content-Type
      // to this’s response’s header list.
      if (contentType && !this.headers.has('content-type')) {
        this.headers.set('content-type', contentType)
      }
    }
  }

  get [Symbol.toStringTag] () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    return this.constructor.name
  }

  // Returns response’s type, e.g., "cors".
  get type () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    // The type getter steps are to return this’s response’s type.
    return this[kState].type
  }

  // Returns response’s URL, if it has one; otherwise the empty string.
  get url () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    // The url getter steps are to return the empty string if this’s
    // response’s URL is null; otherwise this’s response’s URL,
    // serialized with exclude fragment set to true.
    let url = responseURL(this[kState])

    if (url == null) {
      return ''
    }

    if (url.hash) {
      url = new URL(url)
      url.hash = ''
    }

    return url.toString()
  }

  // Returns whether response was obtained through a redirect.
  get redirected () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    // The redirected getter steps are to return true if this’s response’s URL
    // list has more than one item; otherwise false.
    return this[kState].urlList.length > 1
  }

  // Returns response’s status.
  get status () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    // The status getter steps are to return this’s response’s status.
    return this[kState].status
  }

  // Returns whether response’s status is an ok status.
  get ok () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    // The ok getter steps are to return true if this’s response’s status is an
    // ok status; otherwise false.
    return this[kState].status >= 200 && this[kState].status <= 299
  }

  // Returns response’s status message.
  get statusText () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    // The statusText getter steps are to return this’s response’s status
    // message.
    return this[kState].statusText
  }

  // Returns response’s headers as Headers.
  get headers () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    // The headers getter steps are to return this’s headers.
    return this[kHeaders]
  }

  // Returns a clone of response.
  clone () {
    if (!(this instanceof Response)) {
      throw new TypeError('Illegal invocation')
    }

    // 1. If this is unusable, then throw a TypeError.
    if (this.bodyUsed || (this.body && this.body.locked)) {
      throw new TypeError()
    }

    // 2. Let clonedResponse be the result of cloning this’s response.
    const clonedResponse = cloneResponse(this[kState])

    // 3. Return the result of creating a Response object, given
    // clonedResponse, this’s headers’s guard, and this’s relevant Realm.
    const clonedResponseObject = new Response()
    clonedResponseObject[kState] = clonedResponse
    clonedResponseObject[kRealm] = this[kRealm]
    clonedResponseObject[kHeaders][kHeadersList] = clonedResponse.headersList
    clonedResponseObject[kHeaders][kGuard] = this[kHeaders][kGuard]
    clonedResponseObject[kHeaders][kRealm] = this[kHeaders][kRealm]

    return clonedResponseObject
  }
}
mixinBody(Response.prototype)

Object.defineProperties(Response.prototype, {
  type: kEnumerableProperty,
  url: kEnumerableProperty,
  status: kEnumerableProperty,
  ok: kEnumerableProperty,
  redirected: kEnumerableProperty,
  statusText: kEnumerableProperty,
  headers: kEnumerableProperty,
  clone: kEnumerableProperty
})

// https://fetch.spec.whatwg.org/#concept-response-clone
function cloneResponse (response) {
  // To clone a response response, run these steps:

  // 1. If response is a filtered response, then return a new identical
  // filtered response whose internal response is a clone of response’s
  // internal response.
  if (response.internalResponse) {
    return filterResponse(
      cloneResponse(response.internalResponse),
      response.type
    )
  }

  // 2. Let newResponse be a copy of response, except for its body.
  const newResponse = makeResponse({ ...response, body: null })

  // 3. If response’s body is non-null, then set newResponse’s body to the
  // result of cloning response’s body.
  if (response.body != null) {
    newResponse.body = cloneBody(response.body)
  }

  // 4. Return newResponse.
  return newResponse
}

function makeResponse (init) {
  return {
    internalResponse: null,
    aborted: false,
    rangeRequested: false,
    timingAllowPassed: false,
    type: 'default',
    status: 200,
    timingInfo: null,
    cacheState: '',
    statusText: '',
    ...init,
    headersList: init.headersList
      ? new HeadersList(...init.headersList)
      : new HeadersList(),
    urlList: init.urlList ? [...init.urlList] : []
  }
}

function makeNetworkError (reason) {
  return makeResponse({
    type: 'error',
    status: 0,
    error:
      reason instanceof Error
        ? reason
        : new Error(reason ? String(reason) : reason),
    aborted: reason && reason.name === 'AbortError'
  })
}

// https://fetch.spec.whatwg.org/#concept-filtered-response
function filterResponse (response, type) {
  // Set response to the following filtered response with response as its
  // internal response, depending on request’s response tainting:
  if (type === 'basic') {
    // A basic filtered response is a filtered response whose type is "basic"
    // and header list excludes any headers in internal response’s header list
    // whose name is a forbidden response-header name.

    const headers = []
    for (let n = 0; n < response.headersList.length; n += 2) {
      if (!forbiddenResponseHeaderNames.includes(response.headersList[n])) {
        headers.push(response.headersList[n + 0], response.headersList[n + 1])
      }
    }

    return makeResponse({
      ...response,
      internalResponse: response,
      headersList: new HeadersList(...headers),
      type: 'basic'
    })
  } else if (type === 'cors') {
    // A CORS filtered response is a filtered response whose type is "cors"
    // and header list excludes any headers in internal response’s header
    // list whose name is not a CORS-safelisted response-header name, given
    // internal response’s CORS-exposed header-name list.

    // TODO: This is not correct...
    return makeResponse({
      ...response,
      internalResponse: response,
      type: 'cors'
    })
  } else if (type === 'opaque') {
    // An opaque filtered response is a filtered response whose type is
    // "opaque", URL list is the empty list, status is 0, status message
    // is the empty byte sequence, header list is empty, and body is null.

    return makeResponse({
      ...response,
      internalResponse: response,
      type: 'opaque',
      urlList: [],
      status: 0,
      statusText: '',
      body: null
    })
  } else if (type === 'opaqueredirect') {
    // An opaque-redirect filtered response is a filtered response whose type
    // is "opaqueredirect", status is 0, status message is the empty byte
    // sequence, header list is empty, and body is null.

    return makeResponse({
      ...response,
      internalResponse: response,
      type: 'opaqueredirect',
      status: 0,
      statusText: '',
      headersList: new HeadersList(),
      body: null
    })
  } else {
    assert(false)
  }
}

module.exports = { makeNetworkError, makeResponse, filterResponse, Response }
