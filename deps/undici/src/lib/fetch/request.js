/* globals AbortController */

'use strict'

const { extractBody, mixinBody, cloneBody } = require('./body')
const { Headers, fill: fillHeaders, HeadersList } = require('./headers')
const util = require('../core/util')
const {
  isValidHTTPToken,
  sameOrigin,
  toUSVString,
  normalizeMethod
} = require('./util')
const {
  forbiddenMethods,
  corsSafeListedMethods,
  referrerPolicy,
  requestRedirect,
  requestMode,
  requestCredentials,
  requestCache
} = require('./constants')
const { kEnumerableProperty } = util
const { kHeaders, kSignal, kState, kGuard, kRealm } = require('./symbols')
const { kHeadersList } = require('../core/symbols')
const assert = require('assert')

let TransformStream

const kInit = Symbol('init')

const requestFinalizer = new FinalizationRegistry(({ signal, abort }) => {
  signal.removeEventListener('abort', abort)
})

// https://fetch.spec.whatwg.org/#request-class
class Request {
  // https://fetch.spec.whatwg.org/#dom-request
  constructor (...args) {
    if (args[0] === kInit) {
      return
    }

    if (args.length < 1) {
      throw new TypeError(
        `Failed to construct 'Request': 1 argument required, but only ${args.length} present.`
      )
    }
    if (
      args.length >= 1 &&
      typeof args[1] !== 'object' &&
      args[1] !== undefined
    ) {
      throw new TypeError(
        "Failed to construct 'Request': cannot convert to dictionary."
      )
    }
    const input = args[0] instanceof Request ? args[0] : toUSVString(args[0])
    const init = args.length >= 1 ? args[1] ?? {} : {}

    // TODO
    this[kRealm] = { settingsObject: {} }

    // 1. Let request be null.
    let request = null

    // 2. Let fallbackMode be null.
    let fallbackMode = null

    // 3. Let baseURL be this’s relevant settings object’s API base URL.
    const baseUrl = this[kRealm].settingsObject.baseUrl

    // 4. Let signal be null.
    let signal = null

    // 5. If input is a string, then:
    if (typeof input === 'string') {
      // 1. Let parsedURL be the result of parsing input with baseURL.
      // 2. If parsedURL is failure, then throw a TypeError.
      let parsedURL
      try {
        parsedURL = new URL(input, baseUrl)
      } catch (err) {
        throw new TypeError('Failed to parse URL from ' + input, { cause: err })
      }

      // 3. If parsedURL includes credentials, then throw a TypeError.
      if (parsedURL.username || parsedURL.password) {
        throw new TypeError(
          'Request cannot be constructed from a URL that includes credentials: ' +
            input
        )
      }

      // 4. Set request to a new request whose URL is parsedURL.
      request = makeRequest({ urlList: [parsedURL] })

      // 5. Set fallbackMode to "cors".
      fallbackMode = 'cors'
    } else {
      // 6. Otherwise:

      // 7. Assert: input is a Request object.
      assert(input instanceof Request)

      // 8. Set request to input’s request.
      request = input[kState]

      // 9. Set signal to input’s signal.
      signal = input[kSignal]
    }

    // 7. Let origin be this’s relevant settings object’s origin.
    const origin = this[kRealm].settingsObject.origin

    // 8. Let window be "client".
    let window = 'client'

    // 9. If request’s window is an environment settings object and its origin
    // is same origin with origin, then set window to request’s window.
    if (
      request.window?.constructor?.name === 'EnvironmentSettingsObject' &&
      sameOrigin(request.window, origin)
    ) {
      window = request.window
    }

    // 10. If init["window"] exists and is non-null, then throw a TypeError.
    if (init.window !== undefined && init.window != null) {
      throw new TypeError(`'window' option '${window}' must be null`)
    }

    // 11. If init["window"] exists, then set window to "no-window".
    if (init.window !== undefined) {
      window = 'no-window'
    }

    // 12. Set request to a new request with the following properties:
    request = makeRequest({
      // URL request’s URL.
      // undici implementation note: this is set as the first item in request's urlList in makeRequest
      // method request’s method.
      method: request.method,
      // header list A copy of request’s header list.
      // undici implementation note: headersList is cloned in makeRequest
      headersList: request.headersList,
      // unsafe-request flag Set.
      unsafeRequest: request.unsafeRequest,
      // client This’s relevant settings object.
      client: this[kRealm].settingsObject,
      // window window.
      window,
      // priority request’s priority.
      priority: request.priority,
      // origin request’s origin. The propagation of the origin is only significant for navigation requests
      // being handled by a service worker. In this scenario a request can have an origin that is different
      // from the current client.
      origin: request.origin,
      // referrer request’s referrer.
      referrer: request.referrer,
      // referrer policy request’s referrer policy.
      referrerPolicy: request.referrerPolicy,
      // mode request’s mode.
      mode: request.mode,
      // credentials mode request’s credentials mode.
      credentials: request.credentials,
      // cache mode request’s cache mode.
      cache: request.cache,
      // redirect mode request’s redirect mode.
      redirect: request.redirect,
      // integrity metadata request’s integrity metadata.
      integrity: request.integrity,
      // keepalive request’s keepalive.
      keepalive: request.keepalive,
      // reload-navigation flag request’s reload-navigation flag.
      reloadNavigation: request.reloadNavigation,
      // history-navigation flag request’s history-navigation flag.
      historyNavigation: request.historyNavigation,
      // URL list A clone of request’s URL list.
      urlList: [...request.urlList]
    })

    // 13. If init is not empty, then:
    if (Object.keys(init).length > 0) {
      // 1. If request’s mode is "navigate", then set it to "same-origin".
      if (request.mode === 'navigate') {
        request.mode = 'same-origin'
      }

      // 2. Unset request’s reload-navigation flag.
      request.reloadNavigation = false

      // 3. Unset request’s history-navigation flag.
      request.historyNavigation = false

      // 4. Set request’s origin to "client".
      request.origin = 'client'

      // 5. Set request’s referrer to "client"
      request.referrer = 'client'

      // 6. Set request’s referrer policy to the empty string.
      request.referrerPolicy = ''

      // 7. Set request’s URL to request’s current URL.
      request.url = request.urlList[request.urlList.length - 1]

      // 8. Set request’s URL list to « request’s URL ».
      request.urlList = [request.url]
    }

    // 14. If init["referrer"] exists, then:
    if (init.referrer !== undefined) {
      // 1. Let referrer be init["referrer"].
      const referrer = init.referrer

      // 2. If referrer is the empty string, then set request’s referrer to "no-referrer".
      if (referrer === '') {
        request.referrer = 'no-referrer'
      } else {
        // 1. Let parsedReferrer be the result of parsing referrer with
        // baseURL.
        // 2. If parsedReferrer is failure, then throw a TypeError.
        let parsedReferrer
        try {
          parsedReferrer = new URL(referrer, baseUrl)
        } catch (err) {
          throw new TypeError(`Referrer "${referrer}" is not a valid URL.`, { cause: err })
        }

        // 3. If one of the following is true
        // parsedReferrer’s cannot-be-a-base-URL is true, scheme is "about",
        // and path contains a single string "client"
        // parsedReferrer’s origin is not same origin with origin
        // then set request’s referrer to "client".
        // TODO

        // 4. Otherwise, set request’s referrer to parsedReferrer.
        request.referrer = parsedReferrer
      }
    }

    // 15. If init["referrerPolicy"] exists, then set request’s referrer policy
    // to it.
    if (init.referrerPolicy !== undefined) {
      request.referrerPolicy = init.referrerPolicy
      if (!referrerPolicy.includes(request.referrerPolicy)) {
        throw new TypeError(
          `Failed to construct 'Request': The provided value '${request.referrerPolicy}' is not a valid enum value of type ReferrerPolicy.`
        )
      }
    }

    // 16. Let mode be init["mode"] if it exists, and fallbackMode otherwise.
    let mode
    if (init.mode !== undefined) {
      mode = init.mode
      if (!requestMode.includes(mode)) {
        throw new TypeError(
          `Failed to construct 'Request': The provided value '${request.mode}' is not a valid enum value of type RequestMode.`
        )
      }
    } else {
      mode = fallbackMode
    }

    // 17. If mode is "navigate", then throw a TypeError.
    if (mode === 'navigate') {
      throw new TypeError()
    }

    // 18. If mode is non-null, set request’s mode to mode.
    if (mode != null) {
      request.mode = mode
    }

    // 19. If init["credentials"] exists, then set request’s credentials mode
    // to it.
    if (init.credentials !== undefined) {
      request.credentials = init.credentials
      if (!requestCredentials.includes(request.credentials)) {
        throw new TypeError(
          `Failed to construct 'Request': The provided value '${request.credentials}' is not a valid enum value of type RequestCredentials.`
        )
      }
    }

    // 18. If init["cache"] exists, then set request’s cache mode to it.
    if (init.cache !== undefined) {
      request.cache = init.cache
      if (!requestCache.includes(request.cache)) {
        throw new TypeError(
          `Failed to construct 'Request': The provided value '${request.cache}' is not a valid enum value of type RequestCache.`
        )
      }
    }

    // 21. If request’s cache mode is "only-if-cached" and request’s mode is
    // not "same-origin", then throw a TypeError.
    if (request.cache === 'only-if-cached' && request.mode !== 'same-origin') {
      throw new TypeError(
        "'only-if-cached' can be set only with 'same-origin' mode"
      )
    }

    // 22. If init["redirect"] exists, then set request’s redirect mode to it.
    if (init.redirect !== undefined) {
      request.redirect = init.redirect
      if (!requestRedirect.includes(request.redirect)) {
        throw new TypeError(
          `Failed to construct 'Request': The provided value '${request.redirect}' is not a valid enum value of type RequestRedirect.`
        )
      }
    }

    // 23. If init["integrity"] exists, then set request’s integrity metadata to it.
    if (init.integrity !== undefined && init.integrity != null) {
      request.integrity = String(init.integrity)
    }

    // 24. If init["keepalive"] exists, then set request’s keepalive to it.
    if (init.keepalive !== undefined) {
      request.keepalive = Boolean(init.keepalive)
    }

    // 25. If init["method"] exists, then:
    if (init.method !== undefined) {
      // 1. Let method be init["method"].
      let method = init.method

      // 2. If method is not a method or method is a forbidden method, then
      // throw a TypeError.
      if (!isValidHTTPToken(init.method)) {
        throw TypeError(`'${init.method}' is not a valid HTTP method.`)
      }

      if (forbiddenMethods.indexOf(method.toUpperCase()) !== -1) {
        throw TypeError(`'${init.method}' HTTP method is unsupported.`)
      }

      // 3. Normalize method.
      method = normalizeMethod(init.method)

      // 4. Set request’s method to method.
      request.method = method
    }

    // 26. If init["signal"] exists, then set signal to it.
    if (init.signal !== undefined) {
      signal = init.signal
    }

    // 27. Set this’s request to request.
    this[kState] = request

    // 28. Set this’s signal to a new AbortSignal object with this’s relevant
    // Realm.
    const ac = new AbortController()
    this[kSignal] = ac.signal
    this[kSignal][kRealm] = this[kRealm]

    // 29. If signal is not null, then make this’s signal follow signal.
    if (signal != null) {
      if (
        !signal ||
        typeof signal.aborted !== 'boolean' ||
        typeof signal.addEventListener !== 'function'
      ) {
        throw new TypeError(
          "Failed to construct 'Request': member signal is not of type AbortSignal."
        )
      }

      if (signal.aborted) {
        ac.abort()
      } else {
        const abort = () => ac.abort()
        signal.addEventListener('abort', abort, { once: true })
        requestFinalizer.register(this, { signal, abort })
      }
    }

    // 30. Set this’s headers to a new Headers object with this’s relevant
    // Realm, whose header list is request’s header list and guard is
    // "request".
    this[kHeaders] = new Headers()
    this[kHeaders][kGuard] = 'request'
    this[kHeaders][kHeadersList] = request.headersList
    this[kHeaders][kRealm] = this[kRealm]

    // 31. If this’s request’s mode is "no-cors", then:
    if (mode === 'no-cors') {
      // 1. If this’s request’s method is not a CORS-safelisted method,
      // then throw a TypeError.
      if (!corsSafeListedMethods.includes(request.method)) {
        throw new TypeError(
          `'${request.method} is unsupported in no-cors mode.`
        )
      }

      // 2. Set this’s headers’s guard to "request-no-cors".
      this[kHeaders][kGuard] = 'request-no-cors'
    }

    // 32. If init is not empty, then:
    if (Object.keys(init).length !== 0) {
      // 1. Let headers be a copy of this’s headers and its associated header
      // list.
      let headers = new Headers(this.headers)

      // 2. If init["headers"] exists, then set headers to init["headers"].
      if (init.headers !== undefined) {
        headers = init.headers
      }

      // 3. Empty this’s headers’s header list.
      this[kState].headersList = new HeadersList()
      this[kHeaders][kHeadersList] = this[kState].headersList

      // 4. If headers is a Headers object, then for each header in its header
      // list, append header’s name/header’s value to this’s headers.
      if (headers.constructor.name === 'Headers') {
        for (const [key, val] of headers[kHeadersList] || headers) {
          this[kHeaders].append(key, val)
        }
      } else {
        // 5. Otherwise, fill this’s headers with headers.
        fillHeaders(this[kState].headersList, headers)
      }
    }

    // 33. Let inputBody be input’s request’s body if input is a Request
    // object; otherwise null.
    const inputBody = input instanceof Request ? input[kState].body : null

    // 34. If either init["body"] exists and is non-null or inputBody is
    // non-null, and request’s method is `GET` or `HEAD`, then throw a
    // TypeError.
    if (
      ((init.body !== undefined && init.body != null) || inputBody != null) &&
      (request.method === 'GET' || request.method === 'HEAD')
    ) {
      throw new TypeError('Request with GET/HEAD method cannot have body.')
    }

    // 35. Let initBody be null.
    let initBody = null

    // 36. If init["body"] exists and is non-null, then:
    if (init.body !== undefined && init.body != null) {
      // 1. Let Content-Type be null.
      // 2. Set initBody and Content-Type to the result of extracting
      // init["body"], with keepalive set to request’s keepalive.
      const [extractedBody, contentType] = extractBody(
        init.body,
        request.keepalive
      )
      initBody = extractedBody

      // 3, If Content-Type is non-null and this’s headers’s header list does
      // not contain `Content-Type`, then append `Content-Type`/Content-Type to
      // this’s headers.
      if (contentType && !this[kHeaders].has('content-type')) {
        this[kHeaders].append('content-type', contentType)
      }
    }

    // 37. Let inputOrInitBody be initBody if it is non-null; otherwise
    // inputBody.
    const inputOrInitBody = initBody ?? inputBody

    // 38. If inputOrInitBody is non-null and inputOrInitBody’s source is
    // null, then:
    if (inputOrInitBody != null && inputOrInitBody.source == null) {
      // 1. If this’s request’s mode is neither "same-origin" nor "cors",
      // then throw a TypeError.
      if (request.mode !== 'same-origin' && request.mode !== 'cors') {
        throw new TypeError(
          'If request is made from ReadableStream, mode should be "same-origin" or "cors"'
        )
      }

      // 2. Set this’s request’s use-CORS-preflight flag.
      request.useCORSPreflightFlag = true
    }

    // 39. Let finalBody be inputOrInitBody.
    let finalBody = inputOrInitBody

    // 40. If initBody is null and inputBody is non-null, then:
    if (initBody == null && inputBody != null) {
      // 1. If input is unusable, then throw a TypeError.
      if (util.isDisturbed(inputBody.stream) || inputBody.stream.locked) {
        throw new TypeError(
          'Cannot construct a Request with a Request object that has already been used.'
        )
      }

      // 2. Set finalBody to the result of creating a proxy for inputBody.
      if (!TransformStream) {
        TransformStream = require('stream/web').TransformStream
      }

      // https://streams.spec.whatwg.org/#readablestream-create-a-proxy
      const identityTransform = new TransformStream()
      inputBody.stream.pipeThrough(identityTransform)
      finalBody = {
        source: inputBody.source,
        length: inputBody.length,
        stream: identityTransform.readable
      }
    }

    // 41. Set this’s request’s body to finalBody.
    this[kState].body = finalBody
  }

  get [Symbol.toStringTag] () {
    return this.constructor.name
  }

  // Returns request’s HTTP method, which is "GET" by default.
  get method () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The method getter steps are to return this’s request’s method.
    return this[kState].method
  }

  // Returns the URL of request as a string.
  get url () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The url getter steps are to return this’s request’s URL, serialized.
    return this[kState].url.toString()
  }

  // Returns a Headers object consisting of the headers associated with request.
  // Note that headers added in the network layer by the user agent will not
  // be accounted for in this object, e.g., the "Host" header.
  get headers () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The headers getter steps are to return this’s headers.
    return this[kHeaders]
  }

  // Returns the kind of resource requested by request, e.g., "document"
  // or "script".
  get destination () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The destination getter are to return this’s request’s destination.
    return this[kState].destination
  }

  // Returns the referrer of request. Its value can be a same-origin URL if
  // explicitly set in init, the empty string to indicate no referrer, and
  // "about:client" when defaulting to the global’s default. This is used
  // during fetching to determine the value of the `Referer` header of the
  // request being made.
  get referrer () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // 1. If this’s request’s referrer is "no-referrer", then return the
    // empty string.
    if (this[kState].referrer === 'no-referrer') {
      return ''
    }

    // 2. If this’s request’s referrer is "client", then return
    // "about:client".
    if (this[kState].referrer === 'client') {
      return 'about:client'
    }

    // Return this’s request’s referrer, serialized.
    return this[kState].referrer.toString()
  }

  // Returns the referrer policy associated with request.
  // This is used during fetching to compute the value of the request’s
  // referrer.
  get referrerPolicy () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The referrerPolicy getter steps are to return this’s request’s referrer policy.
    return this[kState].referrerPolicy
  }

  // Returns the mode associated with request, which is a string indicating
  // whether the request will use CORS, or will be restricted to same-origin
  // URLs.
  get mode () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The mode getter steps are to return this’s request’s mode.
    return this[kState].mode
  }

  // Returns the credentials mode associated with request,
  // which is a string indicating whether credentials will be sent with the
  // request always, never, or only when sent to a same-origin URL.
  get credentials () {
    // The credentials getter steps are to return this’s request’s credentials mode.
    return this[kState].credentials
  }

  // Returns the cache mode associated with request,
  // which is a string indicating how the request will
  // interact with the browser’s cache when fetching.
  get cache () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The cache getter steps are to return this’s request’s cache mode.
    return this[kState].cache
  }

  // Returns the redirect mode associated with request,
  // which is a string indicating how redirects for the
  // request will be handled during fetching. A request
  // will follow redirects by default.
  get redirect () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The redirect getter steps are to return this’s request’s redirect mode.
    return this[kState].redirect
  }

  // Returns request’s subresource integrity metadata, which is a
  // cryptographic hash of the resource being fetched. Its value
  // consists of multiple hashes separated by whitespace. [SRI]
  get integrity () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The integrity getter steps are to return this’s request’s integrity
    // metadata.
    return this[kState].integrity
  }

  // Returns a boolean indicating whether or not request can outlive the
  // global in which it was created.
  get keepalive () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The keepalive getter steps are to return this’s request’s keepalive.
    return this[kState].keepalive
  }

  // Returns a boolean indicating whether or not request is for a reload
  // navigation.
  get isReloadNavigation () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The isReloadNavigation getter steps are to return true if this’s
    // request’s reload-navigation flag is set; otherwise false.
    return this[kState].reloadNavigation
  }

  // Returns a boolean indicating whether or not request is for a history
  // navigation (a.k.a. back-foward navigation).
  get isHistoryNavigation () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The isHistoryNavigation getter steps are to return true if this’s request’s
    // history-navigation flag is set; otherwise false.
    return this[kState].historyNavigation
  }

  // Returns the signal associated with request, which is an AbortSignal
  // object indicating whether or not request has been aborted, and its
  // abort event handler.
  get signal () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // The signal getter steps are to return this’s signal.
    return this[kSignal]
  }

  // Returns a clone of request.
  clone () {
    if (!(this instanceof Request)) {
      throw new TypeError('Illegal invocation')
    }

    // 1. If this is unusable, then throw a TypeError.
    if (this.bodyUsed || this.body?.locked) {
      throw new TypeError('unusable')
    }

    // 2. Let clonedRequest be the result of cloning this’s request.
    const clonedRequest = cloneRequest(this[kState])

    // 3. Let clonedRequestObject be the result of creating a Request object,
    // given clonedRequest, this’s headers’s guard, and this’s relevant Realm.
    const clonedRequestObject = new Request(kInit)
    clonedRequestObject[kState] = clonedRequest
    clonedRequestObject[kRealm] = this[kRealm]
    clonedRequestObject[kHeaders] = new Headers()
    clonedRequestObject[kHeaders][kHeadersList] = clonedRequest.headersList
    clonedRequestObject[kHeaders][kGuard] = this[kHeaders][kGuard]
    clonedRequestObject[kHeaders][kRealm] = this[kHeaders][kRealm]

    // 4. Make clonedRequestObject’s signal follow this’s signal.
    const ac = new AbortController()
    if (this.signal.aborted) {
      ac.abort()
    } else {
      this.signal.addEventListener(
        'abort',
        function () {
          ac.abort()
        },
        { once: true }
      )
    }
    clonedRequestObject[kSignal] = ac.signal

    // 4. Return clonedRequestObject.
    return clonedRequestObject
  }
}

mixinBody(Request.prototype)

function makeRequest (init) {
  // https://fetch.spec.whatwg.org/#requests
  const request = {
    method: 'GET',
    localURLsOnly: false,
    unsafeRequest: false,
    body: null,
    client: null,
    reservedClient: null,
    replacesClientId: '',
    window: 'client',
    keepalive: false,
    serviceWorkers: 'all',
    initiator: '',
    destination: '',
    priority: null,
    origin: 'client',
    policyContainer: 'client',
    referrer: 'client',
    referrerPolicy: '',
    mode: 'no-cors',
    useCORSPreflightFlag: false,
    credentials: 'same-origin',
    useCredentials: false,
    cache: 'default',
    redirect: 'follow',
    integrity: '',
    cryptoGraphicsNonceMetadata: '',
    parserMetadata: '',
    reloadNavigation: false,
    historyNavigation: false,
    userActivation: false,
    taintedOrigin: false,
    redirectCount: 0,
    responseTainting: 'basic',
    preventNoCacheCacheControlHeaderModification: false,
    done: false,
    timingAllowFailed: false,
    ...init,
    headersList: init.headersList
      ? new HeadersList(init.headersList)
      : new HeadersList()
  }
  request.url = request.urlList[0]
  return request
}

// https://fetch.spec.whatwg.org/#concept-request-clone
function cloneRequest (request) {
  // To clone a request request, run these steps:

  // 1. Let newRequest be a copy of request, except for its body.
  const newRequest = makeRequest({ ...request, body: null })

  // 2. If request’s body is non-null, set newRequest’s body to the
  // result of cloning request’s body.
  if (request.body != null) {
    newRequest.body = cloneBody(request.body)
  }

  // 3. Return newRequest.
  return newRequest
}

Object.defineProperties(Request.prototype, {
  method: kEnumerableProperty,
  url: kEnumerableProperty,
  headers: kEnumerableProperty,
  redirect: kEnumerableProperty,
  clone: kEnumerableProperty,
  signal: kEnumerableProperty
})

module.exports = { Request, makeRequest }
