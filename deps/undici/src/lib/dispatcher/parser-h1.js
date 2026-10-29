'use strict'

const assert = require('node:assert')
const util = require('../core/util.js')
const timers = require('../util/timers.js')
const {
  HeadersTimeoutError,
  HeadersOverflowError,
  SocketError,
  InformationalError,
  BodyTimeoutError,
  HTTPParserError,
  ResponseContentLengthMismatchError,
  ResponseExceededMaxSizeError
} = require('../core/errors.js')
const {
  kUrl,
  kReset,
  kClient,
  kParser,
  kBlocking,
  kRunning,
  kWriting,
  kQueue,
  kKeepAliveDefaultTimeout,
  kRunningIdx,
  kError,
  kPipelining,
  kSocket,
  kKeepAliveTimeoutValue,
  kKeepAliveMaxTimeout,
  kKeepAliveTimeoutThreshold,
  kBodyTimeout,
  kMaxResponseSize,
  kResume,
  kHTTPContext,
  kMaxHeadersSize
} = require('../core/symbols.js')

const FastBuffer = Buffer[Symbol.species]
const removeAllListeners = util.removeAllListeners

const kPausedByHandler = Symbol('kPausedByHandler')

const BUFFER_PAGE_SIZE = 4096
const INITIAL_BUFFER_SIZE = 128 * 1024
const EMPTY_BUF = Buffer.alloc(0)
const BODY_KIND_CONTENT_LENGTH = 0
const EVENT_RANGE_SIZE = 9
const EVENT_HEADERS_SIZE = 19

let miloPackage

// We disable wasm SIMD on ppc64 as it seems to be broken on Power 9 architectures.
let useWasmSIMD = process.arch !== 'ppc64'
// The Env Variable UNDICI_NO_WASM_SIMD allows explicitly overriding the default behavior.
if (process.env.UNDICI_NO_WASM_SIMD === '1') {
  useWasmSIMD = false
} else if (process.env.UNDICI_NO_WASM_SIMD === '0') {
  useWasmSIMD = true
}

const USE_NATIVE_TIMER = 0
const USE_FAST_TIMER = 1
const TIMEOUT_HEADERS = 2 | USE_FAST_TIMER
const TIMEOUT_BODY = 4 | USE_FAST_TIMER
const TIMEOUT_KEEP_ALIVE = 8 | USE_NATIVE_TIMER

let miloEnabled =
  process.env.UNDICI_USE_MILO === '1' || process.env.UNDICI_USE_MILO === 'true'
let miloInstance = null
let currentBuffer = null
let currentBufferSize = 0
let currentBufferPtr = null
let currentBufferRef = null

function onParserTimeout (parserWeakRef) {
  const parser = parserWeakRef.deref ? parserWeakRef.deref() : parserWeakRef
  if (!parser) {
    return
  }

  const { socket, timeoutType, client, paused } = parser

  if (timeoutType === TIMEOUT_HEADERS) {
    if (!socket[kWriting] || socket.writableNeedDrain || client[kRunning] > 1) {
      assert(!paused, 'cannot be paused while waiting for headers')
      util.destroy(socket, new HeadersTimeoutError())
    }
  } else if (timeoutType === TIMEOUT_BODY) {
    if (!paused) {
      util.destroy(socket, new BodyTimeoutError())
    }
  } else if (timeoutType === TIMEOUT_KEEP_ALIVE) {
    assert(client[kRunning] === 0 && client[kKeepAliveTimeoutValue])
    util.destroy(socket, new InformationalError('socket idle timeout'))
  }
}

function getMilo () {
  if (miloInstance) {
    return miloInstance
  }

  // Milo modules compile WASM on load; defer loading until the parser is needed.
  if (useWasmSIMD) {
    try {
      miloPackage = require('../milo/simd.js')
    } catch {}
  }

  miloPackage = miloPackage || require('../milo/no-simd.js')
  miloInstance = miloPackage.setup()
  return miloInstance
}

class H1Parser {
  constructor (client, socket) {
    this.milo = getMilo()

    this.client = client
    this.socket = socket
    this.parser = this.milo.create()
    this.events = new FastBuffer(
      this.milo.memory.buffer,
      this.parser + this.milo.ParserFields.EVENTS,
      4
    ).readUInt32LE(0)

    this.milo.setShouldAutodetect(this.parser, false)
    this.milo.setIsRequest(this.parser, false)
    this.milo.setShouldManageUnconsumed(this.parser, false)
    this.milo.setShouldSuspendAfterHeaders(this.parser, true)
    this.milo.setMaxHeaderLength(this.parser, client[kMaxHeadersSize])
    this.milo.setActiveEvents(
      this.parser,
      this.milo.EVENT_ACTIVE_ON_ERROR |
        this.milo.EVENT_ACTIVE_ON_MESSAGE_COMPLETE |
        this.milo.EVENT_ACTIVE_ON_REASON |
        this.milo.EVENT_ACTIVE_ON_HEADER_NAME |
        this.milo.EVENT_ACTIVE_ON_HEADER_VALUE |
        this.milo.EVENT_ACTIVE_ON_HEADERS |
        this.milo.EVENT_ACTIVE_ON_DATA |
        this.milo.EVENT_ACTIVE_ON_TRAILER_NAME |
        this.milo.EVENT_ACTIVE_ON_TRAILER_VALUE
    )

    this.timeout = null
    this.timeoutWeakRef = new WeakRef(this)
    this.timeoutValue = null
    this.timeoutType = null
    this.statusCode = 0
    this.statusText = ''
    this.upgrade = false
    this.responseStarted = false
    this.pausedByParser = false
    this.pauseStateIsStart = false
    this.headers = []
    this.trailers = []
    this.headersSize = 0
    this.trailersSize = 0
    this.headersMaxSize = client[kMaxHeadersSize]
    this.shouldKeepAlive = false
    this.paused = false
    this.errored = false
    this.waitingForMoreData = 0
    this.resume = this.resume.bind(this)
    this.bytesRead = 0
    this.keepAlive = ''
    this.connection = ''
    this.contentLength = -1
    this.maxResponseSize = client[kMaxResponseSize]
    this.directBodyRemaining = 0
    this.directBodyCompletePending = false
    this.headersSuspended = false
    this.skipBody = false

    this[kPausedByHandler] = false

    if (
      INITIAL_BUFFER_SIZE > currentBufferSize ||
      currentBuffer.buffer !== this.milo.memory.buffer
    ) {
      this.ensureBuffers(INITIAL_BUFFER_SIZE)
    } else {
      this.updateBuffers()
    }
  }

  setTimeout (delay, type) {
    if (
      delay !== this.timeoutValue ||
      (type & USE_FAST_TIMER) ^ (this.timeoutType & USE_FAST_TIMER)
    ) {
      if (this.timeout) {
        timers.clearTimeout(this.timeout)
        this.timeout = null
      }

      if (delay) {
        if (type & USE_FAST_TIMER) {
          this.timeout = timers.setFastTimeout(
            onParserTimeout,
            delay,
            this.timeoutWeakRef
          )
        } else {
          this.timeout = setTimeout(
            onParserTimeout,
            delay,
            this.timeoutWeakRef
          )
          this.timeout?.unref()
        }
      }

      this.timeoutValue = delay
    } else if (this.timeout?.refresh) {
      this.timeout.refresh()
    }

    this.timeoutType = type
  }

  ensureBuffers (size) {
    const { milo } = this

    if (currentBufferPtr != null) {
      milo.dealloc(currentBufferPtr, currentBufferSize)
    }

    // Reallocate buffers
    currentBufferSize = Math.ceil(size / BUFFER_PAGE_SIZE) * BUFFER_PAGE_SIZE
    currentBufferPtr = milo.alloc(currentBufferSize)
    currentBuffer = new FastBuffer(
      milo.memory.buffer,
      currentBufferPtr,
      currentBufferSize
    )

    this.updateBuffers()
  }

  updateBuffers () {
    this.parserFields = new FastBuffer(this.milo.memory.buffer, this.parser)
    this.parserEvents = new FastBuffer(
      this.milo.memory.buffer,
      this.events,
      64 * 1024
    )
  }

  resume () {
    if (this.socket.destroyed || !this.paused) {
      return
    }

    this.milo.resume(this.parser)

    assert(this.timeoutType === TIMEOUT_BODY)
    if (this.timeout?.refresh) {
      this.timeout.refresh()
    }

    this.paused = false
    if (this.directBodyCompletePending) {
      try {
        this.completeDirectBody()
      } catch (err) {
        util.destroy(this.socket, err)
        return
      }

      if (this.paused) {
        return
      }
    }

    this.readMore()
  }

  readMore () {
    if (this.paused || !this.parser) {
      return
    }

    while (!this.paused && this.parser) {
      if (
        this.waitingForMoreData &&
        this.socket.readableLength <= this.waitingForMoreData
      ) {
        break
      }

      this.waitingForMoreData = 0

      const chunk = this.socket.read()
      if (chunk === null) {
        break
      }

      if (this.execute(chunk) === false) {
        break
      }
    }
  }

  execute (data) {
    const available = data.length
    assert(!this.paused)

    const { socket, milo } = this

    try {
      if (this.directBodyRemaining > 0) {
        return this.consumeDirectBody(data)
      }

      if (currentBuffer.buffer !== milo.memory.buffer) {
        currentBuffer = new FastBuffer(
          milo.memory.buffer,
          currentBufferPtr,
          currentBufferSize
        )
        this.updateBuffers()
      } else if (this.parserEvents.buffer !== milo.memory.buffer) {
        this.updateBuffers()
      }

      let offset = 0
      let parseLen = available
      const headersEnd = this.statusCode === 0 ? data.indexOf('\r\n\r\n') : -1

      if (headersEnd !== -1) {
        parseLen = headersEnd + 4
      }

      for (;;) {
        if (parseLen > currentBufferSize) {
          this.ensureBuffers(parseLen)
        }

        const parseData = data.subarray(offset, offset + parseLen)
        currentBuffer.set(parseData)
        currentBufferRef = parseData

        const consumed = milo.parse(this.parser, currentBufferPtr, parseLen)
        this.drainEvents()

        const consumedTotal = offset + consumed

        if (
          this.errored ||
          (consumed === 0 &&
            this.parserFields[milo.ParserFields.ERROR_CODE] !== milo.ERROR_NONE)
        ) {
          throw this.createError(data.subarray(consumedTotal))
        } else if (this.upgrade) {
          this.onUpgrade(data.subarray(consumedTotal))
          return false
        } else if (this.directBodyCompletePending) {
          this.completeDirectBody()

          if (this.paused) {
            return false
          }
        } else if (this.pausedByParser) {
          this.pausedByParser = false
          this.headersSuspended = false
          if (this[kPausedByHandler] && this.pauseStateIsStart) {
            this.pauseStateIsStart = false
            milo.resume(this.parser)
            this[kPausedByHandler] = false
            return false
          }

          this.pauseStateIsStart = false
          this.paused = true
          socket.unshift(data.subarray(consumedTotal))
          return false
        }

        if (this.directBodyRemaining > 0) {
          return this.consumeDirectBody(data.subarray(consumedTotal))
        }

        if (consumed < parseLen) {
          const remaining = data.subarray(consumedTotal)
          socket.unshift(remaining)
          this.waitingForMoreData = consumed === 0 ? remaining.length : 0
          return false
        }

        offset = consumedTotal

        if (offset >= available) {
          if (this.headersSuspended) {
            this.headersSuspended = false
            parseLen = 0
            continue
          }

          return true
        }

        this.headersSuspended = false
        parseLen = available - offset
      }
    } catch (err) {
      util.destroy(socket, err)
      return false
    } finally {
      currentBufferRef = null
    }
  }

  consumeDirectBody (data) {
    const { socket, client, maxResponseSize } = this
    const available = data.length
    const len = Math.min(available, this.directBodyRemaining)

    if (this.timeout?.refresh) {
      this.timeout.refresh()
    }

    if (len > 0) {
      const request = client[kQueue][client[kRunningIdx]]
      assert(request)
      assert(this.timeoutType === TIMEOUT_BODY)
      assert(this.statusCode >= 200)

      if (maxResponseSize > -1 && this.bytesRead + len > maxResponseSize) {
        throw new ResponseExceededMaxSizeError()
      }

      this.bytesRead += len
      this.directBodyRemaining -= len

      if (request.onResponseData(data.subarray(0, len)) === false) {
        this.directBodyCompletePending = this.directBodyRemaining === 0
        this.pauseParser()
        this[kPausedByHandler] = true
        this.pausedByParser = false
        this.paused = true

        if (len < available) {
          socket.unshift(data.subarray(len))
        }

        return false
      }
    }

    if (this.directBodyRemaining === 0) {
      this.completeDirectBody()

      const remaining = data.subarray(len)
      if (
        this.errored ||
        this.parserFields[this.milo.ParserFields.ERROR_CODE] !==
          this.milo.ERROR_NONE
      ) {
        throw this.createError(remaining)
      } else if (this.pausedByParser) {
        this.pausedByParser = false
        this.pauseStateIsStart = false
        this.paused = true
        if (remaining.length > 0) {
          socket.unshift(remaining)
        }
        return false
      }

      if (remaining.length > 0) {
        socket.unshift(remaining)
        return false
      }
    }

    return true
  }

  completeDirectBody () {
    const { milo } = this

    if (currentBuffer.buffer !== milo.memory.buffer) {
      currentBuffer = new FastBuffer(
        milo.memory.buffer,
        currentBufferPtr,
        currentBufferSize
      )
      this.updateBuffers()
    } else if (this.parserEvents.buffer !== milo.memory.buffer) {
      this.updateBuffers()
    }

    this.directBodyCompletePending = false
    currentBufferRef = EMPTY_BUF

    milo.complete(this.parser)
    this.drainEvents()
  }

  setShouldSkipBody (value) {
    if (this.skipBody !== value) {
      this.skipBody = value
      this.milo.setShouldSkipBody(this.parser, value)
    }
  }

  finish () {
    if (!this.parser) {
      return null
    }

    if (this.directBodyRemaining > 0) {
      return new ResponseContentLengthMismatchError()
    }

    if (this.parserEvents.buffer !== this.milo.memory.buffer) {
      this.updateBuffers()
    }

    this.milo.finish(this.parser)
    this.drainEvents()

    if (!this.errored) {
      return null
    }

    return this.createError(EMPTY_BUF)
  }

  createError (data) {
    const { contentLength, bytesRead } = this

    if (contentLength !== -1 && bytesRead !== contentLength) {
      return new ResponseContentLengthMismatchError()
    }

    const errorCode = this.parserFields[this.milo.ParserFields.ERROR_CODE]
    const errorName = this.milo.Errors[errorCode]
    const errorDescription =
      errorName === 'UNEXPECTED_EOF'
        ? 'Invalid EOF state'
        : this.milo.getErrorDescription(this.parser)
    const reason =
      errorName === 'UNEXPECTED_EOF'
        ? errorDescription
        : errorDescription
          ? `[ERROR_${errorName}] ${errorDescription}`
          : `ERROR_${errorName}`
    return new HTTPParserError(
      `Response does not match the HTTP/1.1 protocol (${reason})`,
      errorName,
      data
    )
  }

  destroy () {
    if (this.parser) {
      this.milo.destroy(this.parser)
      this.parser = null
    }

    this.timeout && timers.clearTimeout(this.timeout)
    this.timeout = null
    this.timeoutValue = null
    this.timeoutType = null
    this.paused = false
    this.responseStarted = false
    this.pausedByParser = false
    this.pauseStateIsStart = false
    this.connection = ''
    this.skipBody = false
  }

  pauseParser (stateIsStart = false) {
    this.milo.pause(this.parser)
    this.pausedByParser = true
    this.pauseStateIsStart = stateIsStart
  }

  drainEvents () {
    const { socket, client, milo } = this
    let cursor = 0

    const firstEvent = this.parserEvents[0]
    if (
      firstEvent !== milo.EVENT_END &&
      firstEvent !== milo.EVENT_ERROR &&
      !this.responseStarted
    ) {
      this.responseStarted = true

      if (socket.destroyed) {
        return this.fail(this.milo.ERROR_CALLBACK_ERROR, 'Other side closed')
      }

      const request = client[kQueue][client[kRunningIdx]]
      if (!request) {
        return this.fail(this.milo.ERROR_CALLBACK_ERROR, 'Missing request')
      }

      request.onResponseStarted()
    }

    for (;;) {
      const event = this.parserEvents[cursor]
      let eventLen = EVENT_RANGE_SIZE
      switch (event) {
        case milo.EVENT_END:
          return
        case milo.EVENT_ERROR:
          this.errored = true
          return
        case milo.EVENT_MESSAGE_COMPLETE: {
          const {
            client,
            socket,
            statusCode,
            upgrade,
            trailers,
            contentLength,
            bytesRead,
            shouldKeepAlive
          } = this

          this.responseStarted = false

          if (socket.destroyed && (!statusCode || shouldKeepAlive)) {
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Other side closed')
          }

          if (upgrade) {
            this.pauseParser()
            return
          }

          assert(statusCode >= 100)

          const request = client[kQueue][client[kRunningIdx]]
          assert(request)

          this.statusCode = 0
          this.statusText = ''
          this.bytesRead = 0
          this.contentLength = -1
          this.directBodyRemaining = 0
          this.directBodyCompletePending = false
          this.headersSuspended = false
          this.skipBody = false
          this.keepAlive = ''
          this.connection = ''
          this.trailers = []
          this.trailersSize = 0

          if (statusCode < 200) {
            break
          }

          if (
            request.method !== 'HEAD' &&
            contentLength !== -1 &&
            bytesRead !== contentLength
          ) {
            util.destroy(socket, new ResponseContentLengthMismatchError())
            this.pauseParser()
            break
          }

          request.onResponseEnd(trailers)

          client[kQueue][client[kRunningIdx]++] = null

          if (socket[kWriting]) {
            assert(client[kRunning] === 0)
            util.destroy(socket, new InformationalError('reset'))
            this.pauseParser(true)
          } else if (!shouldKeepAlive) {
            util.destroy(socket, new InformationalError('reset'))
            this.pauseParser(true)
          } else if (socket[kReset] && client[kRunning] === 0) {
            util.destroy(socket, new InformationalError('reset'))
            this.pauseParser(true)
          } else if (client[kPipelining] == null || client[kPipelining] === 1) {
            setImmediate(client[kResume])
          } else {
            client[kResume]()
          }

          break
        }
        case milo.EVENT_REASON: {
          const at = this.parserEvents.readUInt32LE(cursor + 1)
          const len = this.parserEvents.readUInt32LE(cursor + 5)
          this.statusText = new FastBuffer(
            currentBufferRef.buffer,
            currentBufferRef.byteOffset + at,
            len
          ).toString()
          break
        }
        case milo.EVENT_HEADER_NAME: {
          const at = this.parserEvents.readUInt32LE(cursor + 1)
          const len = this.parserEvents.readUInt32LE(cursor + 5)
          const data =
            len > 0
              ? new FastBuffer(
                currentBufferRef.buffer,
                currentBufferRef.byteOffset + at,
                len
              )
              : EMPTY_BUF

          this.headers.push(data)
          this.headersSize += len
          if (this.headersSize >= this.headersMaxSize) {
            util.destroy(this.socket, new HeadersOverflowError())
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Headers overflow')
          }

          break
        }
        case milo.EVENT_HEADER_VALUE: {
          const at = this.parserEvents.readUInt32LE(cursor + 1)
          const len = this.parserEvents.readUInt32LE(cursor + 5)
          const data =
            len > 0
              ? new FastBuffer(
                currentBufferRef.buffer,
                currentBufferRef.byteOffset + at,
                len
              )
              : EMPTY_BUF
          const key = this.headers[this.headers.length - 1]

          this.headers.push(data)

          if (key.length === 10) {
            const headerName = util.bufferToLowerCasedHeaderName(key)
            if (headerName === 'keep-alive') {
              this.keepAlive += (this.keepAlive.length === 0 ? '' : ',') + data.toString()
            } else if (headerName === 'connection') {
              this.connection += (this.connection.length === 0 ? '' : ',') + data.toString()
            }
          }

          this.headersSize += len
          if (this.headersSize >= this.headersMaxSize) {
            util.destroy(this.socket, new HeadersOverflowError())
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Headers overflow')
          }

          break
        }
        case milo.EVENT_HEADERS: {
          eventLen = EVENT_HEADERS_SIZE

          const statusCode = this.parserEvents.readUInt16LE(cursor + 5)
          const shouldKeepAlive = this.parserEvents[cursor + 7] !== 0
          const shouldUpgrade = this.parserEvents[cursor + 8] !== 0
          const hasTrailers = this.parserEvents[cursor + 9] !== 0
          const bodyKind = this.parserEvents[cursor + 10]
          const contentLength =
            this.parserEvents.readUInt32LE(cursor + 11) +
            this.parserEvents.readUInt32LE(cursor + 15) * 0x100000000

          const { socket, client, headers, statusText } = this

          if (socket.destroyed) {
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Other side closed')
          }

          const request = client[kQueue][client[kRunningIdx]]
          if (!request) {
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Missing request')
          }

          assert(!this.upgrade)
          assert(this.statusCode < 200)

          if (statusCode === 100) {
            util.destroy(
              socket,
              new SocketError('bad response', util.getSocketInfo(socket))
            )
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Bad response')
          }

          if (shouldUpgrade && !request.upgrade) {
            util.destroy(
              socket,
              new SocketError('bad upgrade', util.getSocketInfo(socket))
            )
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Bad upgrade')
          }

          assert(this.timeoutType === TIMEOUT_HEADERS)

          this.statusCode = statusCode
          this.shouldKeepAlive =
            shouldKeepAlive ||
            (request.method === 'HEAD' &&
              !socket[kReset] &&
              /(?:^|,)[\t ]*keep-alive[\t ]*(?:,|$)/i.test(this.connection) &&
              !/(?:^|,)[\t ]*close[\t ]*(?:,|$)/i.test(this.connection))
          this.contentLength =
            bodyKind === BODY_KIND_CONTENT_LENGTH ? contentLength : -1

          if (this.statusCode >= 200) {
            const bodyTimeout =
              request.bodyTimeout != null
                ? request.bodyTimeout
                : client[kBodyTimeout]
            this.setTimeout(bodyTimeout, TIMEOUT_BODY)
          } else if (this.timeout?.refresh) {
            this.timeout.refresh()
          }

          if (request.method === 'CONNECT') {
            assert(client[kRunning] === 1)
            milo.setIsConnect(this.parser, true)
            this.upgrade = true
            return
          }

          if (shouldUpgrade) {
            assert(client[kRunning] === 1)
            this.upgrade = true
            this.pauseParser()
            return
          }

          this.headers = []
          this.headersSize = 0

          if (this.shouldKeepAlive && client[kPipelining]) {
            const keepAliveTimeout = this.keepAlive
              ? util.parseKeepAliveTimeout(this.keepAlive)
              : null

            if (keepAliveTimeout != null) {
              const timeout = Math.min(
                keepAliveTimeout - client[kKeepAliveTimeoutThreshold],
                client[kKeepAliveMaxTimeout]
              )
              if (timeout <= 0) {
                socket[kReset] = true
              } else {
                client[kKeepAliveTimeoutValue] = timeout
              }
            } else {
              client[kKeepAliveTimeoutValue] = client[kKeepAliveDefaultTimeout]
            }
          } else {
            socket[kReset] = true
          }

          const pause =
            request.onResponseStart(
              statusCode,
              headers,
              this.resume,
              statusText
            ) === false

          if (request.aborted) {
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Request aborted')
          }

          if (request.method === 'HEAD' || statusCode < 200) {
            this.setShouldSkipBody(true)
          }

          this.headersSuspended = true
          if (
            !pause &&
            request.method !== 'HEAD' &&
            statusCode >= 200 &&
            statusCode !== 204 &&
            statusCode !== 205 &&
            statusCode !== 304 &&
            bodyKind === BODY_KIND_CONTENT_LENGTH &&
            contentLength > 0 &&
            !hasTrailers
          ) {
            this.directBodyRemaining = contentLength
          }

          if (socket[kBlocking]) {
            socket[kBlocking] = false
            client[kResume]()
          }

          if (pause) {
            this.pauseParser()
            this[kPausedByHandler] = true
          } else if (
            request.method === 'HEAD' ||
            statusCode < 200 ||
            statusCode === 204 ||
            statusCode === 205 ||
            statusCode === 304 ||
            (bodyKind === BODY_KIND_CONTENT_LENGTH && contentLength === 0)
          ) {
            this.directBodyCompletePending = true
          }

          break
        }
        case milo.EVENT_DATA: {
          const at = this.parserEvents.readUInt32LE(cursor + 1)
          const len = this.parserEvents.readUInt32LE(cursor + 5)
          const { client, socket, statusCode, maxResponseSize } = this

          if (socket.destroyed) {
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Other side closed')
          }

          const request = client[kQueue][client[kRunningIdx]]
          assert(request)
          assert(this.timeoutType === TIMEOUT_BODY)

          if (this.timeout?.refresh) {
            this.timeout.refresh()
          }

          assert(statusCode >= 200)

          if (maxResponseSize > -1 && this.bytesRead + len > maxResponseSize) {
            util.destroy(socket, new ResponseExceededMaxSizeError())
            return this.fail(
              milo.ERROR_CALLBACK_ERROR,
              'Response size exceeded'
            )
          }

          this.bytesRead += len

          const buffer = currentBufferRef
          if (
            request.onResponseData(
              new FastBuffer(buffer.buffer, buffer.byteOffset + at, len)
            ) === false
          ) {
            this.pauseParser()
            this[kPausedByHandler] = true
          }

          break
        }
        case milo.EVENT_TRAILER_NAME: {
          const at = this.parserEvents.readUInt32LE(cursor + 1)
          const len = this.parserEvents.readUInt32LE(cursor + 5)
          const data =
            len > 0
              ? new FastBuffer(
                currentBufferRef.buffer,
                currentBufferRef.byteOffset + at,
                len
              )
              : EMPTY_BUF

          this.trailers.push(data)

          this.trailersSize += len
          if (this.trailersSize >= this.headersMaxSize) {
            util.destroy(this.socket, new HeadersOverflowError())
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Headers overflow')
          }

          break
        }
        case milo.EVENT_TRAILER_VALUE: {
          const at = this.parserEvents.readUInt32LE(cursor + 1)
          const len = this.parserEvents.readUInt32LE(cursor + 5)
          const data =
            len > 0
              ? new FastBuffer(
                currentBufferRef.buffer,
                currentBufferRef.byteOffset + at,
                len
              )
              : EMPTY_BUF

          this.trailers.push(data)

          this.trailersSize += len
          if (this.trailersSize >= this.headersMaxSize) {
            util.destroy(this.socket, new HeadersOverflowError())
            return this.fail(milo.ERROR_CALLBACK_ERROR, 'Headers overflow')
          }

          break
        }
        default:
          return
      }

      cursor += eventLen
    }
  }

  fail (code, message) {
    this.milo.fail(this.parser, code, message)
    this.errored = true
  }

  onUpgrade (head) {
    const { upgrade, client, socket, headers, statusCode } = this

    assert(upgrade)
    assert(client[kSocket] === socket)
    assert(!socket.destroyed)
    assert(!this.paused)

    const request = client[kQueue][client[kRunningIdx]]
    assert(request)
    assert(request.upgrade || request.method === 'CONNECT')

    this.statusCode = 0
    this.statusText = ''
    this.shouldKeepAlive = false
    this.headers = []
    this.headersSize = 0

    socket.unshift(head)
    socket[kParser].destroy()
    socket[kParser] = null
    socket[kClient] = null
    socket[kError] = null

    removeAllListeners(socket)

    client[kSocket] = null
    client[kHTTPContext] = null
    client[kQueue][client[kRunningIdx]++] = null
    client.emit(
      'disconnect',
      client[kUrl],
      [client],
      new InformationalError('upgrade')
    )

    try {
      request.onRequestUpgrade(statusCode, headers, socket)
    } catch (err) {
      util.destroy(socket, err)
    }

    client[kResume]()
  }
}

function setMiloEnabled (value) {
  miloEnabled = value
}

function getMiloEnabled () {
  return miloEnabled
}

module.exports = { H1Parser, getMiloEnabled, setMiloEnabled }
