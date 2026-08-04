'use strict'

const assert = require('node:assert')
const { pipeline } = require('node:stream')
const util = require('../core/util.js')
const {
  RequestContentLengthMismatchError,
  RequestAbortedError,
  SocketError,
  InformationalError,
  InvalidArgumentError,
  HeadersTimeoutError,
  BodyTimeoutError
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
  kHostAuthority,
  kResume,
  kSize,
  kHTTPContext,
  kClosed,
  kKeepAliveDefaultTimeout,
  kHeadersTimeout,
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
const kRequestStreamState = Symbol('request stream state')
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

function resetHttp2Session (session, err) {
  const client = session[kClient]
  const socket = session[kSocket]

  if (client[kHTTP2Session] === session) {
    client[kSocket] = null
    client[kHTTPContext] = null
    client[kHTTP2Session] = null
  }

  if (socket != null && socket[kError] == null) {
    socket[kError] = err
  }

  if (!session.closed && !session.destroyed) {
    try {
      session.destroy(err)
    } catch {}
  }

  util.destroy(socket, err)
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
  const previousStream = request[kRequestStream]
  detachRequestFromStream(request)
  previousCleanup?.(previousStream)
  request[kRequestStreamId] = stream.id
  request[kRequestStream] = stream
  request[kRequestStreamCleanup] = cleanup
}

function clearRequestStream (request) {
  const cleanup = request[kRequestStreamCleanup]
  const stream = request[kRequestStream]
  detachRequestFromStream(request)
  cleanup?.(stream)
}

function requeueUnsentRequest (client, request) {
  client[kQueue].splice(client[kPendingIdx] + 1, 0, request)
}

function completeRequest (client, request, resetPendingIdx = false) {
  const queue = client[kQueue]
  const runningIdx = client[kRunningIdx]

  // In-order completion: clear the request and advance without splicing.
  // The client's resume loop compacts cleared slots once the index grows.
  if (runningIdx < client[kPendingIdx] && queue[runningIdx] === request) {
    queue[runningIdx] = null
    client[kRunningIdx] = runningIdx + 1
    return
  }

  const index = queue.indexOf(request, runningIdx)

  if (index === -1 || index >= client[kPendingIdx]) {
    return
  }

  queue.splice(index, 1)
  client[kPendingIdx]--

  if (resetPendingIdx && client[kPendingIdx] < client[kRunningIdx]) {
    client[kPendingIdx] = client[kRunningIdx]
  }
}

function canRetryRequestAfterGoAway (request) {
  const { body } = request

  return body == null || util.isBuffer(body) || util.isBlobLike(body)
}

function closeStream (stream, code = NGHTTP2_REFUSED_STREAM) {
  if (stream != null && !stream.destroyed && !stream.closed) {
    try {
      stream.close(code)
    } catch {}
  }
}

function detachRequestStreamForClose (request) {
  const stream = request[kRequestStream]

  clearRequestStream(request)

  return stream
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
    idleTimeout: null,
    // Sockets start out ref'd. Session ref/unref proxies to the socket, so a
    // single cached flag lets us skip redundant uv ref/unref calls, provided
    // every ref/unref of the session or its socket goes through
    // refH2Session/unrefH2Session.
    refed: true,
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
  util.addListener(session, 'goaway', onHttp2SessionGoAway)
  util.addListener(session, 'close', onHttp2SessionClose)
  util.addListener(session, 'remoteSettings', onHttp2RemoteSettings)
  // TODO (@metcoder95): implement SETTINGS support
  // util.addListener(session, 'localSettings', onHttp2RemoteSettings)

  unrefH2Session(session)

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

          // Unlike HTTP/1.1 pipelining, HTTP/2 multiplexes requests on
          // independent streams, so non-idempotent requests can be dispatched
          // concurrently. Retry eligibility is handled by stream/session error
          // handling instead of by serializing all non-idempotent requests.
          // Don't dispatch an upgrade until all preceding requests have completed.
          // Possibly, we do not have remote settings confirmed yet.
          if ((request.upgrade === 'websocket' || request.method === 'CONNECT') && session[kRemoteSettings] === false) return true
        } else {
          return (request.upgrade === 'websocket' || request.method === 'CONNECT') && session[kRemoteSettings] === false
        }
      }

      return false
    }
  }
}

// Session ref/unref proxies to the underlying socket, so refH2Session and
// unrefH2Session cover both and can skip the call when the cached ref state
// already matches.
function refH2Session (session) {
  const state = session[kHTTP2SessionState]

  if (state.refed === false) {
    state.refed = true
    session.ref()
  }
}

function unrefH2Session (session) {
  const state = session[kHTTP2SessionState]

  if (state.refed === true) {
    state.refed = false
    session.unref()
  }
}

function resumeH2 (client) {
  const socket = client[kSocket]
  const session = client[kHTTP2Session]

  if (socket?.destroyed === false) {
    if (client[kSize] === 0 || client[kMaxConcurrentStreams] === 0) {
      unrefH2Session(session)
    } else {
      refH2Session(session)
    }

    if (client[kSize] === 0 && session[kOpenStreams] === 0) {
      setHttp2IdleTimeout(session)
    } else {
      clearHttp2IdleTimeout(session)
    }
  }
}

function clearHttp2IdleTimeout (session) {
  const state = session[kHTTP2SessionState]

  if (state?.idleTimeout != null) {
    clearTimeout(state.idleTimeout)
    state.idleTimeout = null
  }
}

function setHttp2IdleTimeout (session) {
  const client = session[kClient]

  if (client[kHTTP2Session] !== session || session.closed || session.destroyed) {
    return
  }

  if (session[kOpenStreams] !== 0 || client[kSize] !== 0) {
    clearHttp2IdleTimeout(session)
    return
  }

  const state = session[kHTTP2SessionState]
  if (state.idleTimeout == null) {
    state.idleTimeout = setTimeout(onHttp2SessionIdleTimeout, client[kKeepAliveDefaultTimeout], session).unref()
  }
}

function onHttp2SessionIdleTimeout (session) {
  const client = session[kClient]
  const socket = session[kSocket]
  const state = session[kHTTP2SessionState]

  state.idleTimeout = null

  if (client[kHTTP2Session] !== session || session[kOpenStreams] !== 0 || client[kSize] !== 0 || session.closed || session.destroyed) {
    return
  }

  const err = new InformationalError('socket idle timeout')
  socket[kError] = err
  util.destroy(socket, err)
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

  if (this[kReceivedGoAway]) {
    return
  }

  this[kClient][kOnError](err)
}

function onHttp2FrameError (type, code, id) {
  if (id === 0) {
    if (this[kReceivedGoAway]) {
      return
    }

    const err = new InformationalError(`HTTP/2: "frameError" received - type ${type}, code ${code}`)
    this[kSocket][kError] = err
    this[kClient][kOnError](err)
  }
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
  const streamsToClose = []

  // Closing one stream after GOAWAY can synchronously emit frameError on
  // sibling streams. Detach all affected requests first so those errors do
  // not fail requests that are about to be requeued.
  for (let i = pendingIdx; i < previousPendingIdx; i++) {
    const request = client[kQueue][i]

    if (request != null) {
      streamsToClose.push(detachRequestStreamForClose(request))

      if (canRetryRequestAfterGoAway(request)) {
        retriableRequests.push(request)
      } else {
        util.errorRequest(client, request, err)
      }
    }
  }

  for (let i = 0; i < streamsToClose.length; i++) {
    closeStream(streamsToClose[i])
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

  clearHttp2IdleTimeout(this)

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

  clearHttp2IdleTimeout(this)

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
      if (request != null) {
        util.errorRequest(client, request, err)
      }
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

  if (this[kHTTP2Session]?.[kReceivedGoAway]) {
    return
  }

  this[kHTTP2Session]?.[kClient]?.[kOnError](err)
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
    unrefH2Session(session)
    setHttp2IdleTimeout(session)
  }
}

function onUpgradeStreamClose () {
  this.off('error', noop)

  const state = this[kRequestStreamState]
  this[kRequestStreamState] = null

  failUpgradeStream(state, new InformationalError('HTTP/2: stream closed before response headers'))
  closeStreamSession(this)
}

// Idempotent terminal cleanup, called from both 'end' and 'close': the
// null-state guard no-ops the later call.
function completeRequestStream () {
  const state = this[kRequestStreamState]

  if (state == null) {
    return
  }

  // Release the stream first so request references are cleared,
  // then complete the response with trailers if available.
  releaseRequestStream(this)

  if (state.pendingEnd && !state.request.aborted && !state.request.completed) {
    state.request.onResponseEnd(state.trailers || {})
  }

  finalizeRequest(state)
  closeStreamSession(this)
  this[kRequestStreamState] = null
}

// https://www.rfc-editor.org/rfc/rfc7230#section-3.3.2
function shouldSendContentLength (method) {
  return method !== 'GET' && method !== 'HEAD' && method !== 'OPTIONS' && method !== 'TRACE' && method !== 'CONNECT'
}

function buildRequestHeaders (reqHeaders) {
  const headers = {}

  for (let n = 0; n < reqHeaders.length; n += 2) {
    const key = reqHeaders[n + 0]
    const val = reqHeaders[n + 1]
    const current = headers[key]

    if (key === 'cookie') {
      if (current != null) {
        headers[key] = Array.isArray(current) ? (current.push(val), current) : [current, val]
      } else {
        headers[key] = val
      }

      continue
    }

    if (typeof val === 'string') {
      headers[key] = current ? `${current}, ${val}` : val
      continue
    }

    for (let i = 0; i < val.length; i++) {
      headers[key] = headers[key] ? `${headers[key]}, ${val[i]}` : val[i]
    }
  }

  return headers
}

function removeUpgradeStreamListeners (stream) {
  stream.off('response', onUpgradeResponse)
  stream.off('error', onUpgradeStreamError)
  stream.off('end', onUpgradeStreamEnd)
  stream.off('timeout', onUpgradeStreamTimeout)
  stream.off('error', noop)
}

function releaseUpgradeStream (stream) {
  if (stream == null) {
    return
  }

  const state = stream[kRequestStreamState]
  if (state == null) {
    return
  }

  const { request } = state

  if (request[kRequestStream] === stream) {
    detachRequestFromStream(request)
  }

  removeUpgradeStreamListeners(stream)

  if (!stream.destroyed && !stream.closed) {
    stream.once('error', noop)
  }
}

function failUpgradeStream (state, err) {
  if (state == null) {
    return
  }

  const { request } = state
  if (state.responseReceived || request.aborted || request.completed) {
    return
  }

  releaseUpgradeStream(state.stream)
  state.abort(err, true)
}

function onUpgradeStreamError () {
  const state = this[kRequestStreamState]

  if (typeof this.rstCode === 'number' && this.rstCode !== 0) {
    failUpgradeStream(state, new InformationalError(`HTTP/2: "stream error" received - code ${this.rstCode}`))
  } else {
    failUpgradeStream(state, new InformationalError('HTTP/2: stream errored before response headers'))
  }
}

function onUpgradeStreamEnd () {
  failUpgradeStream(this[kRequestStreamState], new InformationalError('HTTP/2: stream half-closed (remote)'))
}

function onUpgradeStreamTimeout () {
  const state = this[kRequestStreamState]
  failUpgradeStream(state, new InformationalError(`HTTP/2: "stream timeout after ${state.headersTimeout}"`))
}

function onUpgradeResponse (headers, _flags) {
  const stream = this
  const state = stream[kRequestStreamState]
  const { request } = state

  state.responseReceived = true

  const statusCode = headers[HTTP2_HEADER_STATUS]
  delete headers[HTTP2_HEADER_STATUS]

  request.onRequestUpgrade(statusCode, headers, stream)

  if (request.aborted || request.completed) {
    return
  }

  removeUpgradeStreamListeners(stream)
  detachRequestFromStream(request)
  finalizeRequest(state)
}

function setupUpgradeStream (stream, state) {
  const { request, headersTimeout, session } = state

  stream[kHTTP2Stream] = true
  stream[kHTTP2Session] = session
  stream[kRequestStreamState] = state
  state.stream = stream

  bindRequestToStream(request, stream, releaseUpgradeStream)
  stream.once('response', onUpgradeResponse)
  stream.on('error', onUpgradeStreamError)
  stream.once('end', onUpgradeStreamEnd)
  stream.on('timeout', onUpgradeStreamTimeout)
  stream.once('close', onUpgradeStreamClose)

  clearHttp2IdleTimeout(session)
  ++session[kOpenStreams]
  stream.setTimeout(headersTimeout)
}

function finalizeRequest (state, resetPendingIdx = false) {
  if (state.requestFinalized) {
    return
  }

  state.requestFinalized = true
  completeRequest(state.client, state.request, resetPendingIdx)

  state.client[kResume]()
}

function openStream (client, request, session, abort, headers, options) {
  try {
    return session.request(headers, options)
  } catch (err) {
    // A GOAWAY'd session rejects new streams, same as an invalid session:
    // reset and requeue on a fresh connection rather than the destroy + abort
    // below, whose destroy(socket, err) can crash via an unhandled 'error'.
    if (err?.code === 'ERR_HTTP2_INVALID_SESSION' || err?.code === 'ERR_HTTP2_GOAWAY_SESSION') {
      const wrappedErr = new SocketError(err.message, util.getSocketInfo(session[kSocket]))
      wrappedErr.cause = err
      session[kError] = wrappedErr
      resetHttp2Session(session, wrappedErr)
      requeueUnsentRequest(client, request)

      return null
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

function writeH2 (client, request) {
  const headersTimeout = request.headersTimeout ?? client[kHeadersTimeout]
  const bodyTimeout = request.bodyTimeout ?? client[kBodyTimeout]
  const session = client[kHTTP2Session]
  const { method, path, host, upgrade, expectContinue, signal, protocol, headers: reqHeaders } = request

  if (upgrade != null && upgrade !== 'websocket') {
    util.errorRequest(client, request, new InvalidArgumentError(`Custom upgrade "${upgrade}" not supported over HTTP/2`))
    return false
  }

  const headers = buildRequestHeaders(reqHeaders)

  headers[HTTP2_HEADER_AUTHORITY] = host || client[kHostAuthority]
  headers[HTTP2_HEADER_METHOD] = method

  // Single pre-shaped state object shared by all stream event handlers.
  // All fields are declared up-front so the object keeps a stable hidden
  // class for the whole request lifetime.
  const state = {
    abort: null,
    body: request.body,
    client,
    contentLength: null,
    expectsPayload: false,
    request,
    headersTimeout,
    bodyTimeout,
    requestFinalized: false,
    responseReceived: false,
    bodySent: false,
    pendingEnd: false,
    trailers: null,
    session,
    stream: null
  }

  const abort = (err, resetPendingIdx = false) => {
    if (request.aborted || request.completed) {
      return
    }

    err = err || new RequestAbortedError()

    util.errorRequest(client, request, err)

    if (state.stream != null) {
      clearRequestStream(request)

      // On Abort, we close the stream to send RST_STREAM frame.
      const stream = state.stream
      stream.close()

      // close() alone leaves cleanup waiting on the 'close' event; on a busy,
      // long-lived multiplexed session that event can fail to fire, leaving the
      // native Http2Stream (and the whole request graph it pins) alive for the
      // session's life. Destroy the stream synchronously to release the handle
      // deterministically. Deferring the destroy (e.g. via setImmediate) leaks
      // the same way when the event loop is stalled and the callback never runs
      // under abort churn (#5558); close() has already queued the RST_STREAM
      // frame on the native session, so a synchronous destroy still sends it.
      if (!stream.destroyed) {
        util.destroy(stream)
      }

      // We move the running index to the next request
      client[kOnError](err)
      finalizeRequest(state, resetPendingIdx)
    }

    // We do not destroy the socket as we can continue using the session
    // the stream gets destroyed and the session remains to create new streams
    util.destroy(state.body, err)
  }

  state.abort = abort

  /** @type {import('node:http2').ClientHttp2Stream} */
  let stream = null

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
    refH2Session(session)

    if (upgrade === 'websocket') {
      // We cannot upgrade to websocket if extended CONNECT protocol is not supported
      if (session[kEnableConnectProtocol] === false) {
        util.errorRequest(client, request, new InformationalError('HTTP/2: Extended CONNECT protocol not supported by server'))
        unrefH2Session(session)
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

      stream = openStream(client, request, session, abort, headers, { endStream: false, signal })
      if (stream == null) {
        unrefH2Session(session)
        return false
      }
      setupUpgradeStream(stream, state)
      return true
    }

    // TODO: consolidate once we support CONNECT properly
    // NOTE: We are already connected, streams are pending, first request
    // will create a new stream. We trigger a request to create the stream and wait until
    // `ready` event is triggered
    // We disabled endStream to allow the user to write to the stream
    stream = openStream(client, request, session, abort, headers, { endStream: false, signal })
    if (stream == null) {
      unrefH2Session(session)
      return false
    }
    setupUpgradeStream(stream, state)

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
    method === 'PATCH' ||
    method === 'QUERY' ||
    method === 'PROPFIND' ||
    method === 'PROPPATCH'
  )

  let body = state.body

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

  refH2Session(session)

  if (channels.sendHeaders.hasSubscribers) {
    let header = ''
    for (const key in headers) {
      header += `${key}: ${headers[key]}\r\n`
    }
    channels.sendHeaders.publish({ request, headers: header, socket: session[kSocket] })
  }

  // TODO(metcoder95): add support for sending trailers
  const shouldEndStream = body === null || contentLength === 0

  state.body = body
  state.contentLength = contentLength
  state.expectsPayload = expectsPayload

  if (expectContinue) {
    headers[HTTP2_HEADER_EXPECT] = '100-continue'
  }

  stream = openStream(client, request, session, abort, headers, { endStream: shouldEndStream, signal })
  if (stream == null) {
    return false
  }
  stream[kHTTP2Stream] = true
  stream[kRequestStreamState] = state
  state.stream = stream

  // Increment counter as we have new streams open
  clearHttp2IdleTimeout(session)
  ++session[kOpenStreams]

  if (headersTimeout) {
    stream.setTimeout(headersTimeout)
  }

  stream[kHTTP2Session] = session
  stream.on('close', completeRequestStream)

  bindRequestToStream(request, stream, releaseRequestStream)
  if (expectContinue) {
    stream.once('continue', writeBodyH2)
  }
  // The handlers below either remove themselves on first invocation or
  // become unreachable once the stream closes, so plain `on` avoids the
  // per-listener `once` wrapper allocation.
  stream.on('response', onResponse)
  stream.on('end', onEnd)
  stream.on('error', onError)
  stream.on('frameError', onFrameError)
  stream.on('aborted', onAborted)
  if (headersTimeout || bodyTimeout) {
    stream.on('timeout', onTimeout)
  }
  stream.on('trailers', onTrailers)

  if (!expectContinue) {
    writeBodyH2.call(stream)
  }

  return true
}

function removeRequestStreamListeners (stream) {
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

function releaseRequestStream (stream) {
  if (stream == null) {
    return
  }

  const state = stream[kRequestStreamState]
  if (state == null) {
    return
  }

  const { request } = state

  if (request[kRequestStream] === stream) {
    detachRequestFromStream(request)
  }

  // A closed or destroyed stream cannot emit further events; leaving the
  // listeners in place saves the removal scans (they are collected with
  // the stream). All handlers bail out when the stream state is gone.
  if (!stream.destroyed && !stream.closed) {
    removeRequestStreamListeners(stream)
    stream.once('error', noop)
  }
}

function onData (chunk) {
  const stream = this
  const state = stream[kRequestStreamState]

  if (state == null) {
    return
  }

  const { request } = state

  if (request.aborted || request.completed) {
    return
  }

  if (request.onResponseData(chunk) === false) {
    stream.pause()
  }
}

function onResponse (headers) {
  const stream = this
  const state = stream[kRequestStreamState]

  if (state == null) {
    return
  }

  const { request } = state

  stream.off('response', onResponse)

  // Final response received while still awaiting 100 (Continue): the body won't
  // be sent, so close our half or the stream stays open and never completes.
  if (state.body != null && !state.bodySent && !stream.writableEnded) {
    stream.removeListener('continue', writeBodyH2)
    stream.end()
  }

  const statusCode = headers[HTTP2_HEADER_STATUS]
  delete headers[HTTP2_HEADER_STATUS]
  request.onResponseStarted()
  state.responseReceived = true

  if (state.headersTimeout || state.bodyTimeout) {
    stream.setTimeout(state.bodyTimeout)
  }

  // Due to the stream nature, it is possible we face a race condition
  // where the stream has been assigned, but the request has been aborted
  // or already completed and headers hasn't been received yet. A late
  // 'response' delivered after completion would call request.onResponseStart
  // post-completion, tripping its `assert(!this.completed)` (an uncatchable
  // throw on the http2 event tick). Guard `completed` here as onEnd/onTrailers
  // already do; best effort is to release the stream immediately as there's
  // no value to keep it open.
  if (request.aborted || request.completed) {
    releaseRequestStream(stream)
    return
  }

  if (request.onResponseStart(Number(statusCode), headers, stream.resume.bind(stream), '') === false) {
    stream.pause()
  }

  stream.on('data', onData)
}

function onEnd () {
  const stream = this
  const state = stream[kRequestStreamState]

  if (state == null) {
    return
  }

  const { request } = state

  stream.off('end', onEnd)

  // onTrailers (which may fire after 'end' on Windows) has already stored
  // trailers on the state by now, so completing here still delivers them.
  if (state.responseReceived) {
    if (!request.aborted && !request.completed) {
      state.pendingEnd = true

      // Complete on 'end': a blocked event loop can keep the stream's 'close'
      // from firing, stranding its buffers until OOM. Idempotent, so a later
      // 'close' no-ops.
      completeRequestStream.call(stream)
    }
  } else {
    // Stream ended without receiving a response - this is an error
    // (e.g., server destroyed the stream before sending headers)
    state.abort(new InformationalError('HTTP/2: stream half-closed (remote)'), true)
  }
}

function onError (err) {
  const stream = this
  const state = stream[kRequestStreamState]

  if (state == null) {
    return
  }

  stream.off('error', onError)
  state.abort(err)
}

function onFrameError (type, code) {
  const stream = this
  const state = stream[kRequestStreamState]

  if (state == null) {
    return
  }

  stream.off('frameError', onFrameError)
  state.abort(new InformationalError(`HTTP/2: "frameError" received - type ${type}, code ${code}`))
}

function onAborted () {
  this.off('data', onData)
}

function onTimeout () {
  const stream = this
  const state = stream[kRequestStreamState]

  if (state == null) {
    return
  }

  // Remove self so timeout doesn't fire again after we handle it
  stream.off('timeout', onTimeout)

  const err = state.responseReceived
    ? new BodyTimeoutError(`HTTP/2: "stream timeout after ${state.bodyTimeout}"`)
    : new HeadersTimeoutError(`HTTP/2: "headers timeout after ${state.headersTimeout}"`)
  state.abort(err)
}

function onTrailers (trailers) {
  const stream = this
  const state = stream[kRequestStreamState]

  if (state == null) {
    return
  }

  const { request } = state

  stream.off('trailers', onTrailers)
  stream.off('data', onData)

  if (request.aborted || request.completed) {
    return
  }

  // Store trailers for completeRequestStream to use when completing
  state.trailers = trailers
}

function writeBodyH2 () {
  const stream = this
  const state = stream[kRequestStreamState]
  state.bodySent = true
  const { abort, body, client, contentLength, expectsPayload, request } = state

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
