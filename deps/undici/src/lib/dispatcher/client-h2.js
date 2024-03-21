'use strict'

const assert = require('node:assert')
const { pipeline } = require('node:stream')
const util = require('../core/util.js')
const {
  RequestContentLengthMismatchError,
  RequestAbortedError,
  SocketError,
  InformationalError
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
  // HTTP2
  kMaxConcurrentStreams,
  kHTTP2Session,
  kResume
} = require('../core/symbols.js')

const kOpenStreams = Symbol('open streams')

// Experimental
let h2ExperimentalWarned = false

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
    HTTP2_HEADER_STATUS
  }
} = http2

async function connectH2 (client, socket) {
  client[kSocket] = socket

  if (!h2ExperimentalWarned) {
    h2ExperimentalWarned = true
    process.emitWarning('H2 support is experimental, expect them to change at any time.', {
      code: 'UNDICI-H2'
    })
  }

  const session = http2.connect(client[kUrl], {
    createConnection: () => socket,
    peerMaxConcurrentStreams: client[kMaxConcurrentStreams]
  })

  session[kOpenStreams] = 0
  session[kClient] = client
  session[kSocket] = socket
  session.on('error', onHttp2SessionError)
  session.on('frameError', onHttp2FrameError)
  session.on('end', onHttp2SessionEnd)
  session.on('goaway', onHTTP2GoAway)
  session.on('close', function () {
    const { [kClient]: client } = this

    const err = this[kError] || new SocketError('closed', util.getSocketInfo(this))

    client[kSocket] = null

    assert(client[kPending] === 0)

    // Fail entire queue.
    const requests = client[kQueue].splice(client[kRunningIdx])
    for (let i = 0; i < requests.length; i++) {
      const request = requests[i]
      errorRequest(client, request, err)
    }

    client[kPendingIdx] = client[kRunningIdx]

    assert(client[kRunning] === 0)

    client.emit('disconnect', client[kUrl], [client], err)

    client[kResume]()
  })
  session.unref()

  client[kHTTP2Session] = session
  socket[kHTTP2Session] = session

  socket.on('error', function (err) {
    assert(err.code !== 'ERR_TLS_CERT_ALTNAME_INVALID')

    this[kError] = err

    this[kClient][kOnError](err)
  })
  socket.on('end', function () {
    util.destroy(this, new SocketError('other side closed', util.getSocketInfo(this)))
  })

  let closed = false
  socket.on('close', () => {
    closed = true
  })

  return {
    version: 'h2',
    defaultPipelining: Infinity,
    write (...args) {
      // TODO (fix): return
      writeH2(client, ...args)
    },
    resume () {

    },
    destroy (err, callback) {
      session.destroy(err)
      if (closed) {
        queueMicrotask(callback)
      } else {
        socket.destroy(err).on('close', callback)
      }
    },
    get destroyed () {
      return socket.destroyed
    },
    busy () {
      return false
    }
  }
}

function onHttp2SessionError (err) {
  assert(err.code !== 'ERR_TLS_CERT_ALTNAME_INVALID')

  this[kSocket][kError] = err

  this[kClient][kOnError](err)
}

function onHttp2FrameError (type, code, id) {
  const err = new InformationalError(`HTTP/2: "frameError" received - type ${type}, code ${code}`)

  if (id === 0) {
    this[kSocket][kError] = err
    this[kClient][kOnError](err)
  }
}

function onHttp2SessionEnd () {
  this.destroy(new SocketError('other side closed'))
  util.destroy(this[kSocket], new SocketError('other side closed'))
}

function onHTTP2GoAway (code) {
  const client = this[kClient]
  const err = new InformationalError(`HTTP/2: "GOAWAY" frame received with code ${code}`)
  client[kSocket] = null
  client[kHTTP2Session] = null

  if (client.destroyed) {
    assert(this[kPending] === 0)

    // Fail entire queue.
    const requests = client[kQueue].splice(client[kRunningIdx])
    for (let i = 0; i < requests.length; i++) {
      const request = requests[i]
      errorRequest(this, request, err)
    }
  } else if (client[kRunning] > 0) {
    // Fail head of pipeline.
    const request = client[kQueue][client[kRunningIdx]]
    client[kQueue][client[kRunningIdx]++] = null

    errorRequest(client, request, err)
  }

  client[kPendingIdx] = client[kRunningIdx]

  assert(client[kRunning] === 0)

  client.emit('disconnect',
    client[kUrl],
    [client],
    err
  )

  client[kResume]()
}

function errorRequest (client, request, err) {
  try {
    request.onError(err)
    assert(request.aborted)
  } catch (err) {
    client.emit('error', err)
  }
}

// https://www.rfc-editor.org/rfc/rfc7230#section-3.3.2
function shouldSendContentLength (method) {
  return method !== 'GET' && method !== 'HEAD' && method !== 'OPTIONS' && method !== 'TRACE' && method !== 'CONNECT'
}

function writeH2 (client, request) {
  const session = client[kHTTP2Session]
  const { body, method, path, host, upgrade, expectContinue, signal, headers: reqHeaders } = request

  if (upgrade) {
    errorRequest(client, request, new Error('Upgrade not supported for H2'))
    return false
  }

  if (request.aborted) {
    return false
  }

  const headers = {}
  for (let n = 0; n < reqHeaders.length; n += 2) {
    const key = reqHeaders[n + 0]
    const val = reqHeaders[n + 1]

    if (Array.isArray(val)) {
      for (let i = 0; i < val.length; i++) {
        if (headers[key]) {
          headers[key] += `,${val[i]}`
        } else {
          headers[key] = val[i]
        }
      }
    } else {
      headers[key] = val
    }
  }

  /** @type {import('node:http2').ClientHttp2Stream} */
  let stream

  const { hostname, port } = client[kUrl]

  headers[HTTP2_HEADER_AUTHORITY] = host || `${hostname}${port ? `:${port}` : ''}`
  headers[HTTP2_HEADER_METHOD] = method

  try {
    // We are already connected, streams are pending.
    // We can call on connect, and wait for abort
    request.onConnect((err) => {
      if (request.aborted || request.completed) {
        return
      }

      err = err || new RequestAbortedError()

      if (stream != null) {
        util.destroy(stream, err)

        session[kOpenStreams] -= 1
        if (session[kOpenStreams] === 0) {
          session.unref()
        }
      }

      errorRequest(client, request, err)
    })
  } catch (err) {
    errorRequest(client, request, err)
  }

  if (method === 'CONNECT') {
    session.ref()
    // We are already connected, streams are pending, first request
    // will create a new stream. We trigger a request to create the stream and wait until
    // `ready` event is triggered
    // We disabled endStream to allow the user to write to the stream
    stream = session.request(headers, { endStream: false, signal })

    if (stream.id && !stream.pending) {
      request.onUpgrade(null, null, stream)
      ++session[kOpenStreams]
    } else {
      stream.once('ready', () => {
        request.onUpgrade(null, null, stream)
        ++session[kOpenStreams]
      })
    }

    stream.once('close', () => {
      session[kOpenStreams] -= 1
      // TODO(HTTP/2): unref only if current streams count is 0
      if (session[kOpenStreams] === 0) session.unref()
    })

    return true
  }

  // https://tools.ietf.org/html/rfc7540#section-8.3
  // :path and :scheme headers must be omitted when sending CONNECT

  headers[HTTP2_HEADER_PATH] = path
  headers[HTTP2_HEADER_SCHEME] = 'https'

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

  if (contentLength == null) {
    contentLength = request.contentLength
  }

  if (contentLength === 0 || !expectsPayload) {
    // https://tools.ietf.org/html/rfc7230#section-3.3.2
    // A user agent SHOULD NOT send a Content-Length header field when
    // the request message does not contain a payload body and the method
    // semantics do not anticipate such a body.

    contentLength = null
  }

  // https://github.com/nodejs/undici/issues/2046
  // A user agent may send a Content-Length header with 0 value, this should be allowed.
  if (shouldSendContentLength(method) && contentLength > 0 && request.contentLength != null && request.contentLength !== contentLength) {
    if (client[kStrictContentLength]) {
      errorRequest(client, request, new RequestContentLengthMismatchError())
      return false
    }

    process.emitWarning(new RequestContentLengthMismatchError())
  }

  if (contentLength != null) {
    assert(body, 'no body must not have content length')
    headers[HTTP2_HEADER_CONTENT_LENGTH] = `${contentLength}`
  }

  session.ref()

  const shouldEndStream = method === 'GET' || method === 'HEAD' || body === null
  if (expectContinue) {
    headers[HTTP2_HEADER_EXPECT] = '100-continue'
    stream = session.request(headers, { endStream: shouldEndStream, signal })

    stream.once('continue', writeBodyH2)
  } else {
    stream = session.request(headers, {
      endStream: shouldEndStream,
      signal
    })
    writeBodyH2()
  }

  // Increment counter as we have new several streams open
  ++session[kOpenStreams]

  stream.once('response', headers => {
    const { [HTTP2_HEADER_STATUS]: statusCode, ...realHeaders } = headers
    request.onResponseStarted()

    if (request.onHeaders(Number(statusCode), realHeaders, stream.resume.bind(stream), '') === false) {
      stream.pause()
    }
  })

  stream.once('end', () => {
    // When state is null, it means we haven't consumed body and the stream still do not have
    // a state.
    // Present specially when using pipeline or stream
    if (stream.state?.state == null || stream.state.state < 6) {
      request.onComplete([])
      return
    }

    // Stream is closed or half-closed-remote (6), decrement counter and cleanup
    // It does not have sense to continue working with the stream as we do not
    // have yet RST_STREAM support on client-side
    session[kOpenStreams] -= 1
    if (session[kOpenStreams] === 0) {
      session.unref()
    }

    const err = new InformationalError('HTTP/2: stream half-closed (remote)')
    errorRequest(client, request, err)
    util.destroy(stream, err)
  })

  stream.on('data', (chunk) => {
    if (request.onData(chunk) === false) {
      stream.pause()
    }
  })

  stream.once('close', () => {
    session[kOpenStreams] -= 1
    // TODO(HTTP/2): unref only if current streams count is 0
    if (session[kOpenStreams] === 0) {
      session.unref()
    }
  })

  stream.once('error', function (err) {
    if (client[kHTTP2Session] && !client[kHTTP2Session].destroyed && !this.closed && !this.destroyed) {
      session[kOpenStreams] -= 1
      util.destroy(stream, err)
    }
  })

  stream.once('frameError', (type, code) => {
    const err = new InformationalError(`HTTP/2: "frameError" received - type ${type}, code ${code}`)
    errorRequest(client, request, err)

    if (client[kHTTP2Session] && !client[kHTTP2Session].destroyed && !this.closed && !this.destroyed) {
      session[kOpenStreams] -= 1
      util.destroy(stream, err)
    }
  })

  // stream.on('aborted', () => {
  //   // TODO(HTTP/2): Support aborted
  // })

  // stream.on('timeout', () => {
  //   // TODO(HTTP/2): Support timeout
  // })

  // stream.on('push', headers => {
  //   // TODO(HTTP/2): Support push
  // })

  // stream.on('trailers', headers => {
  //   // TODO(HTTP/2): Support trailers
  // })

  return true

  function writeBodyH2 () {
    /* istanbul ignore else: assertion */
    if (!body) {
      request.onRequestSent()
    } else if (util.isBuffer(body)) {
      assert(contentLength === body.byteLength, 'buffer body must have content length')
      stream.cork()
      stream.write(body)
      stream.uncork()
      stream.end()
      request.onBodySent(body)
      request.onRequestSent()
    } else if (util.isBlobLike(body)) {
      if (typeof body.stream === 'function') {
        writeIterable({
          client,
          request,
          contentLength,
          h2stream: stream,
          expectsPayload,
          body: body.stream(),
          socket: client[kSocket],
          header: ''
        })
      } else {
        writeBlob({
          body,
          client,
          request,
          contentLength,
          expectsPayload,
          h2stream: stream,
          header: '',
          socket: client[kSocket]
        })
      }
    } else if (util.isStream(body)) {
      writeStream({
        body,
        client,
        request,
        contentLength,
        expectsPayload,
        socket: client[kSocket],
        h2stream: stream,
        header: ''
      })
    } else if (util.isIterable(body)) {
      writeIterable({
        body,
        client,
        request,
        contentLength,
        expectsPayload,
        header: '',
        h2stream: stream,
        socket: client[kSocket]
      })
    } else {
      assert(false)
    }
  }
}

function writeStream ({ h2stream, body, client, request, socket, contentLength, header, expectsPayload }) {
  assert(contentLength !== 0 || client[kRunning] === 0, 'stream body cannot be pipelined')

  // For HTTP/2, is enough to pipe the stream
  const pipe = pipeline(
    body,
    h2stream,
    (err) => {
      if (err) {
        util.destroy(body, err)
        util.destroy(h2stream, err)
      } else {
        request.onRequestSent()
      }
    }
  )

  pipe.on('data', onPipeData)
  pipe.once('end', () => {
    pipe.removeListener('data', onPipeData)
    util.destroy(pipe)
  })

  function onPipeData (chunk) {
    request.onBodySent(chunk)
  }
}

async function writeBlob ({ h2stream, body, client, request, socket, contentLength, header, expectsPayload }) {
  assert(contentLength === body.size, 'blob body must have content length')

  try {
    if (contentLength != null && contentLength !== body.size) {
      throw new RequestContentLengthMismatchError()
    }

    const buffer = Buffer.from(await body.arrayBuffer())

    h2stream.cork()
    h2stream.write(buffer)
    h2stream.uncork()

    request.onBodySent(buffer)
    request.onRequestSent()

    if (!expectsPayload) {
      socket[kReset] = true
    }

    client[kResume]()
  } catch (err) {
    util.destroy(h2stream)
  }
}

async function writeIterable ({ h2stream, body, client, request, socket, contentLength, header, expectsPayload }) {
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
  } catch (err) {
    h2stream.destroy(err)
  } finally {
    request.onRequestSent()
    h2stream.end()
    h2stream
      .off('close', onDrain)
      .off('drain', onDrain)
  }
}

module.exports = connectH2
