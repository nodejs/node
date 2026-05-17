'use strict'

const assert = require('node:assert')
const { pipeline } = require('node:stream')
const util = require('../core/util.js')
const {
  RequestContentLengthMismatchError,
  RequestAbortedError,
  SocketError,
  InformationalError,
  InvalidArgumentError
} = require('../core/errors.js')
const {
  kUrl,
  kReset,
  kClient,
  kRunning,
  kPending,
  kQueue,
  kPendingIdx,
  kRunningIdx,
  kError,
  kSocket,
  kStrictContentLength,
  kOnError,
  kMaxConcurrentStreams,
  kPingInterval,
  kHTTP2Session,
  kHTTP2InitialWindowSize,
  kHTTP2ConnectionWindowSize,
  kResume,
  kSize,
  kHTTPContext,
  kClosed,
  kBodyTimeout,
  kEnableConnectProtocol,
  kRemoteSettings,
  kHTTP2Stream,
  kHTTP2SessionState
} = require('../core/symbols.js')
const { channels } = require('../core/diagnostics.js')

const kOpenStreams = Symbol('open streams')
const kRequestStreamId = Symbol('request stream id')
const kRequestStream = Symbol('request stream')
const kRequestStreamCleanup = Symbol('request stream cleanup')
const kRequestStreamOnData = Symbol('request stream on data')
const kRequestStreamOnCloseError = Symbol('request stream on close error')
const kReceivedGoAway = Symbol('received goaway')

let extractBody

/** @type {import('http2')} */
let http2
try {
  http2 = require('node:http2')
} catch {
  // @ts-ignore
  http2 = { constants: {} }
}

const {
  constants: {
    HTTP2_HEADER_AUTHORITY,
    HTTP2_HEADER_METHOD,
    HTTP2_HEADER_PATH,
    HTTP2_HEADER_SCHEME,
    HTTP2_HEADER_CONTENT_LENGTH,
    HTTP2_HEADER_EXPECT,
    HTTP2_HEADER_STATUS,
    HTTP2_HEADER_PROTOCOL,
    NGHTTP2_NO_ERROR,
    NGHTTP2_REFUSED_STREAM
  }
} = http2

function getGoAwayError (session, errorCode) {
  return session[kError] ||
    (errorCode === NGHTTP2_NO_ERROR
      ? new InformationalError(`HTTP/2: "GOAWAY" frame received with code ${errorCode}`)
      : new SocketError(`HTTP/2: "GOAWAY" frame received with code ${errorCode}`, util.getSocketInfo(session[kSocket])))
}

function getGoAwayPendingIdx (client, lastStreamID) {
  const maxAcceptedStreamID = Number.isInteger(lastStreamID) ? lastStreamID : Number.MAX_SAFE_INTEGER

  for (let i = client[kRunningIdx]; i < client[kPendingIdx]; i++) {
    const request = client[kQueue][i]

    if (request == null) {
      continue
    }

    if (typeof request[kRequestStreamId] !== 'number' || request[kRequestStreamId] > maxAcceptedStreamID) {
      return i
    }
  }

  return client[kPendingIdx]
}

function detachRequestFromStream (request) {
  request[kRequestStreamId] = null
  request[kRequestStream] = null
  request[kRequestStreamCleanup] = null
}

function bindRequestToStream (request, stream, cleanup) {
  const previousCleanup = request[kRequestStreamCleanup]
  detachRequestFromStream(request)
  previousCleanup?.()
  request[kRequestStreamId] = stream.id
  request[kRequestStream] = stream
  request[kRequestStreamCleanup] = cleanup
}

function clearRequestStream (request) {
  const cleanup = request[kRequestStreamCleanup]
  detachRequestFromStream(request)
  cleanup?.()
}

function canRetryRequestAfterGoAway (request) {
  const { body } = request

  return body == null || util.isBuffer(body) || util.isBlobLike(body)
}

function closeRequestStream (request, code = NGHTTP2_REFUSED_STREAM) {
  const stream = request[kRequestStream]

  clearRequestStream(request)

  if (stream != null && !stream.destroyed && !stream.closed) {
    try {
      stream.close(code)
    } catch {}
  }
}

function connectH2 (client, socket) {
  client[kSocket] = socket

  const http2InitialWindowSize = client[kHTTP2InitialWindowSize]
  const http2ConnectionWindowSize = client[kHTTP2ConnectionWindowSize]

  const session = http2.connect(client[kUrl], {
    createConnection: () => socket,
    peerMaxConcurrentStreams: client[kMaxConcurrentStreams],
    settings: {
      // TODO(metcoder95): add support for PUSH
      enablePush: false,
      ...(http2InitialWindowSize != null ? { initialWindowSize: http2InitialWindowSize } : null)
    }
  })

  client[kSocket] = socket
  session[kOpenStreams] = 0
  session[kClient] = client
  session[kSocket] = socket
  session[kHTTP2SessionState] = {
    ping: {
      interval: client[kPingInterval] === 0 ? null : setInterval(onHttp2SendPing, client[kPingInterval], session).unref()
    }
  }
  session[kReceivedGoAway] = false
  // We set it to true by default in a best-effort; however once connected to an H2 server
  // we will check if extended CONNECT protocol is supported or not
  // and set this value accordingly.
  session[kEnableConnectProtocol] = false
  // States whether or not we have received the remote settings from the server
  session[kRemoteSettings] = false

  // Apply connection-level flow control once connected (if supported).
  if (http2ConnectionWindowSize) {
    util.addListener(session, 'connect', applyConnectionWindowSize.bind(session, http2ConnectionWindowSize))
  }

  util.addListener(session, 'error', onHttp2SessionError)
  util.addListener(session, 'frameError', onHttp2FrameError)
  util.addListener(session, 'end', onHttp2SessionEnd)
  util.addListener(session, 'goaway', onHttp2SessionGoAway)
  util.addListener(session, 'close', onHttp2SessionClose)
  util.addListener(session, 'remoteSettings', onHttp2RemoteSettings)
  // TODO (@metcoder95): implement SETTINGS support
  // util.addListener(session, 'localSettings', onHttp2RemoteSettings)

  session.unref()

  client[kHTTP2Session] = session
  socket[kHTTP2Session] = session

  util.addListener(socket, 'error', onHttp2SocketError)
  util.addListener(socket, 'end', onHttp2SocketEnd)
  util.addListener(socket, 'close', onHttp2SocketClose)

  socket[kClosed] = false
  socket.on('close', onSocketClose)

  return {
    version: 'h2',
    defaultPipelining: Infinity,
    /**
     * @param {import('../core/request.js')} request
     * @returns {boolean}
    */
    write (request) {
      return writeH2(client, request)
    },
    /**
     * @returns {void}
     */
    resume () {
      resumeH2(client)
    },
    /**
     * @param {Error | null} err
     * @param {() => void} callback
     */
    destroy (err, callback) {
      if (socket[kClosed]) {
        queueMicrotask(callback)
      } else {
        socket.destroy(err).on('close', callback)
      }
    },
    /**
     * @type {boolean}
     */
    get destroyed () {
      return socket.destroyed
    },
    /**
     * @param {import('../core/request.js')} request
     * @returns {boolean}
    */
    busy (request) {
      if (session[kRemoteSettings] === false && client[kRunning] > 0) {
        return true
      }

      if (client[kRunning] >= client[kMaxConcurrentStreams]) {
        return true
      }

      if (request != null) {
        if (client[kRunning] > 0) {
          // We are already processing requests

          // Non-idempotent request cannot be retried.
          // Ensure that no other requests are inflight and
          // could cause failure.
          if (request.idempotent === false) return true
          // Don't dispatch an upgrade until all preceding requests have completed.
          // Possibly, we do not have remote settings confirmed yet.
          if ((request.upgrade === 'websocket' || request.method === 'CONNECT') && session[kRemoteSettings] === false) return true
          // Request with stream or iterator body can error while other requests
          // are inflight and indirectly error those as well.
          // Ensure this doesn't happen by waiting for inflight
          // to complete before dispatching.

          // Request with stream or iterator body cannot be retried.
          // Ensure that no other requests are inflight and
          // could cause failure.
          if (util.bodyLength(request.body) !== 0 &&
            (util.isStream(request.body) || util.isAsyncIterable(request.body) || util.isFormDataLike(request.body))) return true
        } else {
          return (request.upgrade === 'websocket' || request.method === 'CONNECT') && session[kRemoteSettings] === false
        }
      }

      return false
    }
  }
}

function resumeH2 (client) {
  const socket = client[kSocket]

  if (socket?.destroyed === false) {
    if (client[kSize] === 0 || client[kMaxConcurrentStreams] === 0) {
      socket.unref()
      client[kHTTP2Session].unref()
    } else {
      socket.ref()
      client[kHTTP2Session].ref()
    }
  }
}

function applyConnectionWindowSize (connectionWindowSize) {
  try {
    if (typeof this.setLocalWindowSize === 'function') {
      this.setLocalWindowSize(connectionWindowSize)
    }
  } catch {
    // Best-effort only.
  }
}

function onHttp2RemoteSettings (settings) {
  // Fallbacks are a safe bet, remote setting will always override
  this[kClient][kMaxConcurrentStreams] = settings.maxConcurrentStreams ?? this[kClient][kMaxConcurrentStreams]
  /**
   * From RFC-8441
   * A sender MUST NOT send a SETTINGS_ENABLE_CONNECT_PROTOCOL parameter
   * with the value of 0 after previously sending a value of 1.
   */
  // Note: Cannot be tested in Node, it does not supports disabling the extended CONNECT protocol once enabled
  if (this[kRemoteSettings] === true && this[kEnableConnectProtocol] === true && settings.enableConnectProtocol === false) {
    const err = new InformationalError('HTTP/2: Server disabled extended CONNECT protocol against RFC-8441')
    this[kSocket][kError] = err
    this[kClient][kOnError](err)
    return
  }

  this[kEnableConnectProtocol] = settings.enableConnectProtocol ?? this[kEnableConnectProtocol]
  this[kRemoteSettings] = true
  this[kClient][kResume]()
}

function onHttp2SendPing (session) {
  const state = session[kHTTP2SessionState]
  if ((session.closed || session.destroyed) && state.ping.interval != null) {
    clearInterval(state.ping.interval)
    state.ping.interval = null
    return
  }

  // If no ping sent, do nothing
  session.ping(onPing.bind(session))

  function onPing (err, duration) {
    const client = this[kClient]
    const socket = this[kSocket]

    if (err != null) {
      const error = new InformationalError(`HTTP/2: "PING" errored - type ${err.message}`)
      socket[kError] = error
      client[kOnError](error)
    } else {
      client.emit('ping', duration)
    }
  }
}

function onHttp2SessionError (err) {
  assert(err.code !== 'ERR_TLS_CERT_ALTNAME_INVALID')

  this[kSocket][kError] = err
  this[kClient][kOnError](err)
}

function onHttp2FrameError (type, code, id) {
  if (id === 0) {
    const err = new InformationalError(`HTTP/2: "frameError" received - type ${type}, code ${code}`)
    this[kSocket][kError] = err
    this[kClient][kOnError](err)
  }
}

function onHttp2SessionEnd () {
  const err = new SocketError('other side closed', util.getSocketInfo(this[kSocket]))
  this.destroy(err)
  util.destroy(this[kSocket], err)
}

/**
 * This is the root cause of #3011
 * We need to handle GOAWAY frames properly, and trigger the session close
 * along with the socket right away
 *
 * @this {import('http2').ClientHttp2Session}
 * @param {number} errorCode
 * @param {number} lastStreamID
 */
function onHttp2SessionGoAway (errorCode, lastStreamID) {
  if (this[kReceivedGoAway]) {
    return
  }

  this[kReceivedGoAway] = true

  const err = getGoAwayError(this, errorCode)
  const client = this[kClient]
  const previousPendingIdx = client[kPendingIdx]
  const pendingIdx = getGoAwayPendingIdx(client, lastStreamID)
  const retriableRequests = []

  for (let i = pendingIdx; i < previousPendingIdx; i++) {
    const request = client[kQueue][i]

    if (request != null) {
      closeRequestStream(request)

      if (canRetryRequestAfterGoAway(request)) {
        retriableRequests.push(request)
      } else {
        util.errorRequest(client, request, err)
      }
    }
  }

  if (pendingIdx !== previousPendingIdx) {
    const remainingPendingRequests = client[kQueue].slice(previousPendingIdx)
    client[kQueue].length = pendingIdx
    client[kQueue].push(...retriableRequests, ...remainingPendingRequests)
  }

  if (client[kHTTP2Session] === this) {
    client[kSocket] = null
    client[kHTTPContext] = null
    client[kHTTP2Session] = null
  }

  if (!this.closed && !this.destroyed) {
    this.close()
  }

  client[kPendingIdx] = pendingIdx

  client.emit('disconnect', client[kUrl], [client], err)

  client[kResume]()
}

function onHttp2SessionClose () {
  const { [kClient]: client, [kHTTP2SessionState]: state, [kSocket]: socket } = this

  const err = socket[kError] || this[kError] || new SocketError('closed', util.getSocketInfo(socket))

  if (client[kHTTP2Session] === this) {
    client[kSocket] = null
    client[kHTTPContext] = null
    client[kHTTP2Session] = null
  }

  if (state.ping.interval != null) {
    clearInterval(state.ping.interval)
    state.ping.interval = null
  }

  if (client.destroyed) {
    assert(client[kPending] === 0)

    // Fail entire queue.
    const requests = client[kQueue].splice(client[kRunningIdx])
    for (let i = 0; i < requests.length; i++) {
      const request = requests[i]
      util.errorRequest(client, request, err)
    }
  }
}

function onHttp2SocketClose () {
  const err = this[kError] || new SocketError('closed', util.getSocketInfo(this))

  const session = this[kHTTP2Session]
  const client = session[kClient]

  if (client[kSocket] !== this) {
    // Ignore stale socket closes from a detached GOAWAY session and from any
    // session that has already been replaced. If the session was detached
    // without a GOAWAY and there is no replacement yet, we still need the
    // close event to flush the client state.
    if (session[kReceivedGoAway] || (client[kHTTP2Session] != null && client[kHTTP2Session] !== session)) {
      return
    }
  }

  client[kSocket] = null
  client[kHTTPContext] = null
  if (client[kHTTP2Session] === session) {
    client[kHTTP2Session] = null
  }

  session.destroy(err)

  client[kPendingIdx] = client[kRunningIdx]

  assert(client[kRunning] === 0)

  client.emit('disconnect', client[kUrl], [client], err)

  client[kResume]()
}

function onHttp2SocketError (err) {
  assert(err.code !== 'ERR_TLS_CERT_ALTNAME_INVALID')

  this[kError] = err

  this[kClient][kOnError](err)
}

function onHttp2SocketEnd () {
  util.destroy(this, new SocketError('other side closed', util.getSocketInfo(this)))
}

function onSocketClose () {
  this[kClosed] = true
}

function noop () {}

function closeStreamSession (stream) {
  const session = stream[kHTTP2Session]

  stream[kHTTP2Session] = null
  session[kOpenStreams] -= 1
  if (session[kOpenStreams] === 0) {
    session.unref()
  }
}

function onUpgradeStreamClose () {
  this.off('error', noop)

  const failUpgradeStream = this[kRequestStreamOnCloseError]
  this[kRequestStreamOnCloseError] = null

  failUpgradeStream(new InformationalError('HTTP/2: stream closed before response headers'))
  closeStreamSession(this)
}

function onRequestStreamClose () {
  const onData = this[kRequestStreamOnData]

  this[kRequestStreamOnData] = null
  this.off('data', onData)
  this.off('error', noop)
  closeStreamSession(this)
}

// https://www.rfc-editor.org/rfc/rfc7230#section-3.3.2
function shouldSendContentLength (method) {
  return method !== 'GET' && method !== 'HEAD' && method !== 'OPTIONS' && method !== 'TRACE' && method !== 'CONNECT'
}

function writeH2 (client, request) {
  const requestTimeout = request.bodyTimeout ?? client[kBodyTimeout]
  const session = client[kHTTP2Session]
  const { method, path, host, upgrade, expectContinue, signal, protocol, headers: reqHeaders } = request
  let { body } = request

  if (upgrade != null && upgrade !== 'websocket') {
    util.errorRequest(client, request, new InvalidArgumentError(`Custom upgrade "${upgrade}" not supported over HTTP/2`))
    return false
  }

  const headers = {}
  for (let n = 0; n < reqHeaders.length; n += 2) {
    const key = reqHeaders[n + 0]
    const val = reqHeaders[n + 1]

    if (key === 'cookie') {
      if (headers[key] != null) {
        headers[key] = Array.isArray(headers[key]) ? (headers[key].push(val), headers[key]) : [headers[key], val]
      } else {
        headers[key] = val
      }

      continue
    }

    if (Array.isArray(val)) {
      for (let i = 0; i < val.length; i++) {
        if (headers[key]) {
          headers[key] += `, ${val[i]}`
        } else {
          headers[key] = val[i]
        }
      }
    } else if (headers[key]) {
      headers[key] += `, ${val}`
    } else {
      headers[key] = val
    }
  }

  /** @type {import('node:http2').ClientHttp2Stream} */
  let stream = null

  const { hostname, port } = client[kUrl]

  headers[HTTP2_HEADER_AUTHORITY] = host || `${hostname}${port ? `:${port}` : ''}`
  headers[HTTP2_HEADER_METHOD] = method

  let requestFinalized = false
  const finalizeRequest = (resetPendingIdx = false) => {
    if (requestFinalized) {
      return
    }

    requestFinalized = true
    client[kQueue][client[kRunningIdx]++] = null

    if (resetPendingIdx) {
      client[kPendingIdx] = client[kRunningIdx]
    }

    client[kResume]()
  }

  const abort = (err, resetPendingIdx = false) => {
    if (request.aborted || request.completed) {
      return
    }

    err = err || new RequestAbortedError()

    util.errorRequest(client, request, err)

    if (stream != null) {
      clearRequestStream(request)

      // On Abort, we close the stream to send RST_STREAM frame
      stream.close()

      // We move the running index to the next request
      client[kOnError](err)
      finalizeRequest(resetPendingIdx)
    }

    // We do not destroy the socket as we can continue using the session
    // the stream gets destroyed and the session remains to create new streams
    util.destroy(body, err)
  }

  const requestStream = (headers, options) => {
    try {
      return session.request(headers, options)
    } catch (err) {
      if (err?.code !== 'ERR_HTTP2_INVALID_CONNECTION_HEADERS') {
        throw err
      }

      const wrappedErr = new InformationalError(err.message, { cause: err })
      session[kError] = wrappedErr
      session[kSocket][kError] = wrappedErr

      session.destroy(wrappedErr)
      util.destroy(session[kSocket], wrappedErr)
      abort(wrappedErr)

      return null
    }
  }

  try {
    // We are already connected, streams are pending.
    // We can call on connect, and wait for abort
    request.onRequestStart(abort, null)
  } catch (err) {
    util.errorRequest(client, request, err)
  }

  if (request.aborted) {
    return false
  }

  if (upgrade || method === 'CONNECT') {
    session.ref()

    const setupUpgradeStream = (stream) => {
      let responseReceived = false

      const removeUpgradeStreamListeners = () => {
        stream.off('response', onUpgradeResponse)
        stream.off('error', onUpgradeStreamError)
        stream.off('end', onUpgradeStreamEnd)
        stream.off('timeout', onUpgradeStreamTimeout)
        stream.off('error', noop)
      }

      const releaseUpgradeStream = () => {
        if (request[kRequestStream] === stream) {
          detachRequestFromStream(request)
        }

        removeUpgradeStreamListeners()

        if (!stream.destroyed && !stream.closed) {
          stream.once('error', noop)
        }
      }

      const failUpgradeStream = (err) => {
        if (responseReceived || request.aborted || request.completed) {
          return
        }

        releaseUpgradeStream()
        abort(err, true)
      }

      const onUpgradeStreamError = () => {
        if (typeof stream.rstCode === 'number' && stream.rstCode !== 0) {
          failUpgradeStream(new InformationalError(`HTTP/2: "stream error" received - code ${stream.rstCode}`))
        } else {
          failUpgradeStream(new InformationalError('HTTP/2: stream errored before response headers'))
        }
      }

      const onUpgradeStreamEnd = () => {
        failUpgradeStream(new InformationalError('HTTP/2: stream half-closed (remote)'))
      }

      const onUpgradeStreamTimeout = () => {
        failUpgradeStream(new InformationalError(`HTTP/2: "stream timeout after ${requestTimeout}"`))
      }

      const onUpgradeResponse = (headers, _flags) => {
        responseReceived = true

        const statusCode = headers[HTTP2_HEADER_STATUS]
        delete headers[HTTP2_HEADER_STATUS]

        request.onRequestUpgrade(statusCode, headers, stream)

        if (request.aborted || request.completed) {
          return
        }

        removeUpgradeStreamListeners()
        detachRequestFromStream(request)
        finalizeRequest()
      }

      bindRequestToStream(request, stream, releaseUpgradeStream)
      stream.once('response', onUpgradeResponse)
      stream.on('error', onUpgradeStreamError)
      stream.once('end', onUpgradeStreamEnd)
      stream.on('timeout', onUpgradeStreamTimeout)
      stream[kHTTP2Session] = session
      stream[kRequestStreamOnCloseError] = failUpgradeStream
      stream.once('close', onUpgradeStreamClose)

      ++session[kOpenStreams]
      stream.setTimeout(requestTimeout)
    }

    if (upgrade === 'websocket') {
      // We cannot upgrade to websocket if extended CONNECT protocol is not supported
      if (session[kEnableConnectProtocol] === false) {
        util.errorRequest(client, request, new InformationalError('HTTP/2: Extended CONNECT protocol not supported by server'))
        session.unref()
        return false
      }

      // We force the method to CONNECT
      // as per RFC-8441
      // https://datatracker.ietf.org/doc/html/rfc8441#section-4
      headers[HTTP2_HEADER_METHOD] = 'CONNECT'
      headers[HTTP2_HEADER_PROTOCOL] = 'websocket'
      // :path and :scheme headers must be omitted when sending CONNECT but set if extended-CONNECT
      headers[HTTP2_HEADER_PATH] = path

      if (protocol === 'ws:' || protocol === 'wss:') {
        headers[HTTP2_HEADER_SCHEME] = protocol === 'ws:' ? 'http' : 'https'
      } else {
        headers[HTTP2_HEADER_SCHEME] = protocol === 'http:' ? 'http' : 'https'
      }

      stream = requestStream(headers, { endStream: false, signal })
      if (stream == null) {
        session.unref()
        return false
      }
      stream[kHTTP2Stream] = true
      setupUpgradeStream(stream)
      return true
    }

    // TODO: consolidate once we support CONNECT properly
    // NOTE: We are already connected, streams are pending, first request
    // will create a new stream. We trigger a request to create the stream and wait until
    // `ready` event is triggered
    // We disabled endStream to allow the user to write to the stream
    stream = requestStream(headers, { endStream: false, signal })
    if (stream == null) {
      session.unref()
      return false
    }
    stream[kHTTP2Stream] = true
    setupUpgradeStream(stream)

    return true
  }

  // https://tools.ietf.org/html/rfc7540#section-8.3
  // :path and :scheme headers must be omitted when sending CONNECT
  headers[HTTP2_HEADER_PATH] = path
  headers[HTTP2_HEADER_SCHEME] = protocol === 'http:' ? 'http' : 'https'

  // https://tools.ietf.org/html/rfc7231#section-4.3.1
  // https://tools.ietf.org/html/rfc7231#section-4.3.2
  // https://tools.ietf.org/html/rfc7231#section-4.3.5

  // Sending a payload body on a request that does not
  // expect it can cause undefined behavior on some
  // servers and corrupt connection state. Do not
  // re-use the connection for further requests.

  const expectsPayload = (
    method === 'PUT' ||
    method === 'POST' ||
    method === 'PATCH'
  )

  if (body && typeof body.read === 'function') {
    // Try to read EOF in order to get length.
    body.read(0)
  }

  let contentLength = util.bodyLength(body)

  if (util.isFormDataLike(body)) {
    extractBody ??= require('../web/fetch/body.js').extractBody

    const [bodyStream, contentType] = extractBody(body)
    headers['content-type'] = contentType

    body = bodyStream.stream
    contentLength = bodyStream.length
  }

  if (contentLength == null) {
    contentLength = request.contentLength
  }

  if (contentLength === 0 && !expectsPayload) {
    // https://tools.ietf.org/html/rfc7230#section-3.3.2
    // A user agent SHOULD NOT send a Content-Length header field when
    // the request message does not contain a payload body and the method
    // semantics do not anticipate such a body.
    // And for methods that don't expect a payload, omit Content-Length.
    contentLength = null
  }

  // https://github.com/nodejs/undici/issues/2046
  // A user agent may send a Content-Length header with 0 value, this should be allowed.
  if (shouldSendContentLength(method) && contentLength > 0 && request.contentLength != null && request.contentLength !== contentLength) {
    if (client[kStrictContentLength]) {
      util.errorRequest(client, request, new RequestContentLengthMismatchError())
      return false
    }

    process.emitWarning(new RequestContentLengthMismatchError())
  }

  if (contentLength != null) {
    assert(body || contentLength === 0, 'no body must not have content length')
    headers[HTTP2_HEADER_CONTENT_LENGTH] = `${contentLength}`
  }

  session.ref()

  if (channels.sendHeaders.hasSubscribers) {
    let header = ''
    for (const key in headers) {
      header += `${key}: ${headers[key]}\r\n`
    }
    channels.sendHeaders.publish({ request, headers: header, socket: session[kSocket] })
  }

  // TODO(metcoder95): add support for sending trailers
  const shouldEndStream = body === null
  if (expectContinue) {
    headers[HTTP2_HEADER_EXPECT] = '100-continue'
    stream = requestStream(headers, { endStream: shouldEndStream, signal })
    if (stream == null) {
      return false
    }
    stream[kHTTP2Stream] = true
    bindRequestToStream(request, stream, null)
  } else {
    stream = requestStream(headers, {
      endStream: shouldEndStream,
      signal
    })
    if (stream == null) {
      return false
    }
    stream[kHTTP2Stream] = true
    bindRequestToStream(request, stream, null)
  }

  // Increment counter as we have new streams open
  ++session[kOpenStreams]
  stream.setTimeout(requestTimeout)

  // Track whether we received a response (headers)
  let responseReceived = false
  const onData = (chunk) => {
    if (request.aborted || request.completed) {
      return
    }

    if (request.onResponseData(chunk) === false) {
      stream.pause()
    }
  }

  const removeRequestStreamListeners = () => {
    stream.off('error', noop)
    stream.off('continue', writeBodyH2)
    stream.off('response', onResponse)
    stream.off('end', onEnd)
    stream.off('error', onError)
    stream.off('frameError', onFrameError)
    stream.off('aborted', onAborted)
    stream.off('timeout', onTimeout)
    stream.off('trailers', onTrailers)
    stream.off('data', onData)
  }

  const releaseRequestStream = () => {
    if (request[kRequestStream] === stream) {
      detachRequestFromStream(request)
    }

    removeRequestStreamListeners()

    if (!stream.destroyed && !stream.closed) {
      stream.once('error', noop)
    }
  }

  const onResponse = (headers) => {
    stream.off('response', onResponse)

    const statusCode = headers[HTTP2_HEADER_STATUS]
    delete headers[HTTP2_HEADER_STATUS]
    request.onResponseStarted()
    responseReceived = true

    // Due to the stream nature, it is possible we face a race condition
    // where the stream has been assigned, but the request has been aborted
    // the request remains in-flight and headers hasn't been received yet
    // for those scenarios, best effort is to destroy the stream immediately
    // as there's no value to keep it open.
    if (request.aborted) {
      releaseRequestStream()
      return
    }

    if (request.onResponseStart(Number(statusCode), headers, stream.resume.bind(stream), '') === false) {
      stream.pause()
    }

    stream.on('data', onData)
  }

  const onEnd = () => {
    stream.off('end', onEnd)

    releaseRequestStream()
    // If we received a response, this is a normal completion
    if (responseReceived) {
      if (!request.aborted && !request.completed) {
        request.onResponseEnd({})
      }

      finalizeRequest()
    } else {
      // Stream ended without receiving a response - this is an error
      // (e.g., server destroyed the stream before sending headers)
      abort(new InformationalError('HTTP/2: stream half-closed (remote)'), true)
    }
  }

  stream[kHTTP2Session] = session
  stream[kRequestStreamOnData] = onData
  stream.once('close', onRequestStreamClose)

  const onError = function (err) {
    stream.off('error', onError)

    releaseRequestStream()
    abort(err)
  }

  const onFrameError = (type, code) => {
    stream.off('frameError', onFrameError)

    releaseRequestStream()
    abort(new InformationalError(`HTTP/2: "frameError" received - type ${type}, code ${code}`))
  }

  const onAborted = () => {
    stream.off('data', onData)
  }

  const onTimeout = () => {
    releaseRequestStream()

    const err = new InformationalError(`HTTP/2: "stream timeout after ${requestTimeout}"`)
    abort(err)
  }

  const onTrailers = (trailers) => {
    stream.off('trailers', onTrailers)

    if (request.aborted || request.completed) {
      return
    }

    releaseRequestStream()
    request.onResponseEnd(trailers)
    finalizeRequest()
  }

  bindRequestToStream(request, stream, releaseRequestStream)
  if (expectContinue) {
    stream.once('continue', writeBodyH2)
  }
  stream.once('response', onResponse)
  stream.once('end', onEnd)
  stream.once('error', onError)
  stream.once('frameError', onFrameError)
  stream.on('aborted', onAborted)
  stream.on('timeout', onTimeout)
  stream.once('trailers', onTrailers)

  if (!expectContinue) {
    writeBodyH2()
  }

  return true

  function writeBodyH2 () {
    if (!body || contentLength === 0) {
      writeBuffer(
        abort,
        stream,
        null,
        client,
        request,
        client[kSocket],
        contentLength,
        expectsPayload
      )
    } else if (util.isBuffer(body)) {
      writeBuffer(
        abort,
        stream,
        body,
        client,
        request,
        client[kSocket],
        contentLength,
        expectsPayload
      )
    } else if (util.isBlobLike(body)) {
      if (typeof body.stream === 'function') {
        writeIterable(
          abort,
          stream,
          body.stream(),
          client,
          request,
          client[kSocket],
          contentLength,
          expectsPayload
        )
      } else {
        writeBlob(
          abort,
          stream,
          body,
          client,
          request,
          client[kSocket],
          contentLength,
          expectsPayload
        )
      }
    } else if (util.isStream(body)) {
      writeStream(
        abort,
        client[kSocket],
        expectsPayload,
        stream,
        body,
        client,
        request,
        contentLength
      )
    } else if (util.isIterable(body)) {
      writeIterable(
        abort,
        stream,
        body,
        client,
        request,
        client[kSocket],
        contentLength,
        expectsPayload
      )
    } else {
      assert(false)
    }
  }
}

function writeBuffer (abort, h2stream, body, client, request, socket, contentLength, expectsPayload) {
  try {
    if (body != null && util.isBuffer(body)) {
      assert(contentLength === body.byteLength, 'buffer body must have content length')
      h2stream.cork()
      h2stream.write(body)
      h2stream.uncork()
      h2stream.end()

      request.onBodySent(body)
    }

    if (!expectsPayload) {
      socket[kReset] = true
    }

    request.onRequestSent()
    client[kResume]()
  } catch (error) {
    abort(error)
  }
}

function writeStream (abort, socket, expectsPayload, h2stream, body, client, request, contentLength) {
  assert(contentLength !== 0 || client[kRunning] === 0, 'stream body cannot be pipelined')

  // For HTTP/2, is enough to pipe the stream
  const pipe = pipeline(
    body,
    h2stream,
    (err) => {
      if (err) {
        util.destroy(pipe, err)
        abort(err)
      } else {
        util.removeAllListeners(pipe)
        request.onRequestSent()

        if (!expectsPayload) {
          socket[kReset] = true
        }

        client[kResume]()
      }
    }
  )

  util.addListener(pipe, 'data', onPipeData)

  function onPipeData (chunk) {
    request.onBodySent(chunk)
  }
}

async function writeBlob (abort, h2stream, body, client, request, socket, contentLength, expectsPayload) {
  assert(contentLength === body.size, 'blob body must have content length')

  try {
    if (contentLength != null && contentLength !== body.size) {
      throw new RequestContentLengthMismatchError()
    }

    const buffer = Buffer.from(await body.arrayBuffer())

    h2stream.cork()
    h2stream.write(buffer)
    h2stream.uncork()
    h2stream.end()

    request.onBodySent(buffer)
    request.onRequestSent()

    if (!expectsPayload) {
      socket[kReset] = true
    }

    client[kResume]()
  } catch (err) {
    abort(err)
  }
}

async function writeIterable (abort, h2stream, body, client, request, socket, contentLength, expectsPayload) {
  assert(contentLength !== 0 || client[kRunning] === 0, 'iterator body cannot be pipelined')

  let callback = null
  function onDrain () {
    if (callback) {
      const cb = callback
      callback = null
      cb()
    }
  }

  const waitForDrain = () => new Promise((resolve, reject) => {
    assert(callback === null)

    if (socket[kError]) {
      reject(socket[kError])
    } else {
      callback = resolve
    }
  })

  h2stream
    .on('close', onDrain)
    .on('drain', onDrain)

  try {
    // It's up to the user to somehow abort the async iterable.
    for await (const chunk of body) {
      if (socket[kError]) {
        throw socket[kError]
      }

      const res = h2stream.write(chunk)
      request.onBodySent(chunk)
      if (!res) {
        await waitForDrain()
      }
    }

    h2stream.end()

    request.onRequestSent()

    if (!expectsPayload) {
      socket[kReset] = true
    }

    client[kResume]()
  } catch (err) {
    abort(err)
  } finally {
    h2stream
      .off('close', onDrain)
      .off('drain', onDrain)
  }
}

module.exports = connectH2
