'use strict'

const { Headers, HeadersList, fill } = require('./headers')
const { extractBody, cloneBody, mixinBody } = require('./body')
const util = require('../core/util')
const { kEnumerableProperty } = util
const {
  responseURL,
  isValidReasonPhrase,
  isCancelled,
  isAborted,
  isBlobLike,
  serializeJavascriptValueToJSONString
} = require('./util')
const {
  redirectStatus,
  nullBodyStatus,
  DOMException
} = require('./constants')
const { kState, kHeaders, kGuard, kRealm } = require('./symbols')
const { webidl } = require('./webidl')
const { FormData } = require('./formdata')
const { kHeadersList } = require('../core/symbols')
const assert = require('assert')
const { types } = require('util')

const ReadableStream = globalThis.ReadableStream || require('stream/web').ReadableStream

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

  // https://fetch.spec.whatwg.org/#dom-response-json
  static json (data, init = {}) {
    if (arguments.length === 0) {
      throw new TypeError(
        'Failed to execute \'json\' on \'Response\': 1 argument required, but 0 present.'
      )
    }

    if (init !== null) {
      init = webidl.converters.ResponseInit(init)
    }

    // 1. Let bytes the result of running serialize a JavaScript value to JSON bytes on data.
    const bytes = new TextEncoder('utf-8').encode(
      serializeJavascriptValueToJSONString(data)
    )

    // 2. Let body be the result of extracting bytes.
    const body = extractBody(bytes)

    // 3. Let responseObject be the result of creating a Response object, given a new response,
    //    "response", and this’s relevant Realm.
    const relevantRealm = { settingsObject: {} }
    const responseObject = new Response()
    responseObject[kRealm] = relevantRealm
    responseObject[kHeaders][kGuard] = 'response'
    responseObject[kHeaders][kRealm] = relevantRealm

    // 4. Perform initialize a response given responseObject, init, and (body, "application/json").
    initializeResponse(responseObject, init, { body: body[0], type: 'application/json' })

    // 5. Return responseObject.
    return responseObject
  }

  // Creates a redirect Response that redirects to url with status status.
  static redirect (url, status = 302) {
    const relevantRealm = { settingsObject: {} }

    if (arguments.length < 1) {
      throw new TypeError(
        `Failed to execute 'redirect' on 'Response': 1 argument required, but only ${arguments.length} present.`
      )
    }

    url = webidl.converters.USVString(url)
    status = webidl.converters['unsigned short'](status)

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
    responseObject[kState].headersList.append('location', value)

    // 8. Return responseObject.
    return responseObject
  }

  // https://fetch.spec.whatwg.org/#dom-response
  constructor (body = null, init = {}) {
    if (body !== null) {
      body = webidl.converters.BodyInit(body)
    }

    init = webidl.converters.ResponseInit(init)

    // TODO
    this[kRealm] = { settingsObject: {} }

    // 1. Set this’s response to a new response.
    this[kState] = makeResponse({})

    // 2. Set this’s headers to a new Headers object with this’s relevant
    // Realm, whose header list is this’s response’s header list and guard
    // is "response".
    this[kHeaders] = new Headers()
    this[kHeaders][kGuard] = 'response'
    this[kHeaders][kHeadersList] = this[kState].headersList
    this[kHeaders][kRealm] = this[kRealm]

    // 3. Let bodyWithType be null.
    let bodyWithType = null

    // 4. If body is non-null, then set bodyWithType to the result of extracting body.
    if (body != null) {
      const [extractedBody, type] = extractBody(body)
      bodyWithType = { body: extractedBody, type }
    }

    // 5. Perform initialize a response given this, init, and bodyWithType.
    initializeResponse(this, init, bodyWithType)
  }

  get [Symbol.toStringTag] () {
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
      webidl.errors.exception({
        header: 'Response.clone',
        message: 'Body has already been consumed.'
      })
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

mixinBody(Response)

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
    aborted: false,
    rangeRequested: false,
    timingAllowPassed: false,
    requestIncludesCredentials: false,
    type: 'default',
    status: 200,
    timingInfo: null,
    cacheState: '',
    statusText: '',
    ...init,
    headersList: init.headersList
      ? new HeadersList(init.headersList)
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
        : new Error(reason ? String(reason) : reason, {
          cause: reason instanceof Error ? reason : undefined
        }),
    aborted: reason && reason.name === 'AbortError'
  })
}

function makeFilteredResponse (response, state) {
  state = {
    internalResponse: response,
    ...state
  }

  return new Proxy(response, {
    get (target, p) {
      return p in state ? state[p] : target[p]
    },
    set (target, p, value) {
      assert(!(p in state))
      target[p] = value
      return true
    }
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

    // Note: undici does not implement forbidden response-header names
    return makeFilteredResponse(response, {
      type: 'basic',
      headersList: response.headersList
    })
  } else if (type === 'cors') {
    // A CORS filtered response is a filtered response whose type is "cors"
    // and header list excludes any headers in internal response’s header
    // list whose name is not a CORS-safelisted response-header name, given
    // internal response’s CORS-exposed header-name list.

    // Note: undici does not implement CORS-safelisted response-header names
    return makeFilteredResponse(response, {
      type: 'cors',
      headersList: response.headersList
    })
  } else if (type === 'opaque') {
    // An opaque filtered response is a filtered response whose type is
    // "opaque", URL list is the empty list, status is 0, status message
    // is the empty byte sequence, header list is empty, and body is null.

    return makeFilteredResponse(response, {
      type: 'opaque',
      urlList: Object.freeze([]),
      status: 0,
      statusText: '',
      body: null
    })
  } else if (type === 'opaqueredirect') {
    // An opaque-redirect filtered response is a filtered response whose type
    // is "opaqueredirect", status is 0, status message is the empty byte
    // sequence, header list is empty, and body is null.

    return makeFilteredResponse(response, {
      type: 'opaqueredirect',
      status: 0,
      statusText: '',
      headersList: [],
      body: null
    })
  } else {
    assert(false)
  }
}

// https://fetch.spec.whatwg.org/#appropriate-network-error
function makeAppropriateNetworkError (fetchParams) {
  // 1. Assert: fetchParams is canceled.
  assert(isCancelled(fetchParams))

  // 2. Return an aborted network error if fetchParams is aborted;
  // otherwise return a network error.
  return isAborted(fetchParams)
    ? makeNetworkError(new DOMException('The operation was aborted.', 'AbortError'))
    : makeNetworkError(fetchParams.controller.terminated.reason)
}

// https://whatpr.org/fetch/1392.html#initialize-a-response
function initializeResponse (response, init, body) {
  // 1. If init["status"] is not in the range 200 to 599, inclusive, then
  //    throw a RangeError.
  if (init.status !== null && (init.status < 200 || init.status > 599)) {
    throw new RangeError('init["status"] must be in the range of 200 to 599, inclusive.')
  }

  // 2. If init["statusText"] does not match the reason-phrase token production,
  //    then throw a TypeError.
  if ('statusText' in init && init.statusText != null) {
    // See, https://datatracker.ietf.org/doc/html/rfc7230#section-3.1.2:
    //   reason-phrase  = *( HTAB / SP / VCHAR / obs-text )
    if (!isValidReasonPhrase(String(init.statusText))) {
      throw new TypeError('Invalid statusText')
    }
  }

  // 3. Set response’s response’s status to init["status"].
  if ('status' in init && init.status != null) {
    response[kState].status = init.status
  }

  // 4. Set response’s response’s status message to init["statusText"].
  if ('statusText' in init && init.statusText != null) {
    response[kState].statusText = init.statusText
  }

  // 5. If init["headers"] exists, then fill response’s headers with init["headers"].
  if ('headers' in init && init.headers != null) {
    fill(response[kState].headersList, init.headers)
  }

  // 6. If body was given, then:
  if (body) {
    // 1. If response's status is a null body status, then throw a TypeError.
    if (nullBodyStatus.includes(response.status)) {
      webidl.errors.exception({
        header: 'Response constructor',
        message: 'Invalid response status code.'
      })
    }

    // 2. Set response's body to body's body.
    response[kState].body = body.body

    // 3. If body's type is non-null and response's header list does not contain
    //    `Content-Type`, then append (`Content-Type`, body's type) to response's header list.
    if (body.type != null && !response[kState].headersList.has('Content-Type')) {
      response[kState].headersList.append('content-type', body.type)
    }
  }
}

webidl.converters.ReadableStream = webidl.interfaceConverter(
  ReadableStream
)

webidl.converters.FormData = webidl.interfaceConverter(
  FormData
)

webidl.converters.URLSearchParams = webidl.interfaceConverter(
  URLSearchParams
)

// https://fetch.spec.whatwg.org/#typedefdef-xmlhttprequestbodyinit
webidl.converters.XMLHttpRequestBodyInit = function (V) {
  if (typeof V === 'string') {
    return webidl.converters.USVString(V)
  }

  if (isBlobLike(V)) {
    return webidl.converters.Blob(V)
  }

  if (
    types.isAnyArrayBuffer(V) ||
    types.isTypedArray(V) ||
    types.isDataView(V)
  ) {
    return webidl.converters.BufferSource(V)
  }

  if (V instanceof FormData) {
    return webidl.converters.FormData(V)
  }

  if (V instanceof URLSearchParams) {
    return webidl.converters.URLSearchParams(V)
  }

  return webidl.converters.DOMString(V)
}

// https://fetch.spec.whatwg.org/#bodyinit
webidl.converters.BodyInit = function (V) {
  if (V instanceof ReadableStream) {
    return webidl.converters.ReadableStream(V)
  }

  // Note: the spec doesn't include async iterables,
  // this is an undici extension.
  if (V?.[Symbol.asyncIterator]) {
    return V
  }

  return webidl.converters.XMLHttpRequestBodyInit(V)
}

webidl.converters.ResponseInit = webidl.dictionaryConverter([
  {
    key: 'status',
    converter: webidl.converters['unsigned short'],
    defaultValue: 200
  },
  {
    key: 'statusText',
    converter: webidl.converters.ByteString,
    defaultValue: ''
  },
  {
    key: 'headers',
    converter: webidl.converters.HeadersInit
  }
])

module.exports = {
  makeNetworkError,
  makeResponse,
  makeAppropriateNetworkError,
  filterResponse,
  Response
}
