'use strict'

const {
  InvalidArgumentError,
  NotSupportedError
} = require('./errors')
const assert = require('assert')
const util = require('./util')

// tokenRegExp and headerCharRegex have been lifted from
// https://github.com/nodejs/node/blob/main/lib/_http_common.js

/**
 * Verifies that the given val is a valid HTTP token
 * per the rules defined in RFC 7230
 * See https://tools.ietf.org/html/rfc7230#section-3.2.6
 */
const tokenRegExp = /^[\^_`a-zA-Z\-0-9!#$%&'*+.|~]+$/

/**
 * Matches if val contains an invalid field-vchar
 *  field-value    = *( field-content / obs-fold )
 *  field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
 *  field-vchar    = VCHAR / obs-text
 */
const headerCharRegex = /[^\t\x20-\x7e\x80-\xff]/

// Verifies that a given path is valid does not contain control chars \x00 to \x20
const invalidPathRegex = /[^\u0021-\u00ff]/

const kHandler = Symbol('handler')

const channels = {}

let extractBody

const nodeVersion = process.versions.node.split('.')
const nodeMajor = Number(nodeVersion[0])
const nodeMinor = Number(nodeVersion[1])

try {
  const diagnosticsChannel = require('diagnostics_channel')
  channels.create = diagnosticsChannel.channel('undici:request:create')
  channels.bodySent = diagnosticsChannel.channel('undici:request:bodySent')
  channels.headers = diagnosticsChannel.channel('undici:request:headers')
  channels.trailers = diagnosticsChannel.channel('undici:request:trailers')
  channels.error = diagnosticsChannel.channel('undici:request:error')
} catch {
  channels.create = { hasSubscribers: false }
  channels.bodySent = { hasSubscribers: false }
  channels.headers = { hasSubscribers: false }
  channels.trailers = { hasSubscribers: false }
  channels.error = { hasSubscribers: false }
}

class Request {
  constructor (origin, {
    path,
    method,
    body,
    headers,
    query,
    idempotent,
    blocking,
    upgrade,
    headersTimeout,
    bodyTimeout,
    throwOnError
  }, handler) {
    if (typeof path !== 'string') {
      throw new InvalidArgumentError('path must be a string')
    } else if (
      path[0] !== '/' &&
      !(path.startsWith('http://') || path.startsWith('https://')) &&
      method !== 'CONNECT'
    ) {
      throw new InvalidArgumentError('path must be an absolute URL or start with a slash')
    } else if (invalidPathRegex.exec(path) !== null) {
      throw new InvalidArgumentError('invalid request path')
    }

    if (typeof method !== 'string') {
      throw new InvalidArgumentError('method must be a string')
    } else if (tokenRegExp.exec(method) === null) {
      throw new InvalidArgumentError('invalid request method')
    }

    if (upgrade && typeof upgrade !== 'string') {
      throw new InvalidArgumentError('upgrade must be a string')
    }

    if (headersTimeout != null && (!Number.isFinite(headersTimeout) || headersTimeout < 0)) {
      throw new InvalidArgumentError('invalid headersTimeout')
    }

    if (bodyTimeout != null && (!Number.isFinite(bodyTimeout) || bodyTimeout < 0)) {
      throw new InvalidArgumentError('invalid bodyTimeout')
    }

    this.headersTimeout = headersTimeout

    this.bodyTimeout = bodyTimeout

    this.throwOnError = throwOnError === true

    this.method = method

    if (body == null) {
      this.body = null
    } else if (util.isStream(body)) {
      this.body = body
    } else if (util.isBuffer(body)) {
      this.body = body.byteLength ? body : null
    } else if (ArrayBuffer.isView(body)) {
      this.body = body.buffer.byteLength ? Buffer.from(body.buffer, body.byteOffset, body.byteLength) : null
    } else if (body instanceof ArrayBuffer) {
      this.body = body.byteLength ? Buffer.from(body) : null
    } else if (typeof body === 'string') {
      this.body = body.length ? Buffer.from(body) : null
    } else if (util.isFormDataLike(body) || util.isIterable(body) || util.isBlobLike(body)) {
      this.body = body
    } else {
      throw new InvalidArgumentError('body must be a string, a Buffer, a Readable stream, an iterable, or an async iterable')
    }

    this.completed = false

    this.aborted = false

    this.upgrade = upgrade || null

    this.path = query ? util.buildURL(path, query) : path

    this.origin = origin

    this.idempotent = idempotent == null
      ? method === 'HEAD' || method === 'GET'
      : idempotent

    this.blocking = blocking == null ? false : blocking

    this.host = null

    this.contentLength = null

    this.contentType = null

    this.headers = ''

    if (Array.isArray(headers)) {
      if (headers.length % 2 !== 0) {
        throw new InvalidArgumentError('headers array must be even')
      }
      for (let i = 0; i < headers.length; i += 2) {
        processHeader(this, headers[i], headers[i + 1])
      }
    } else if (headers && typeof headers === 'object') {
      const keys = Object.keys(headers)
      for (let i = 0; i < keys.length; i++) {
        const key = keys[i]
        processHeader(this, key, headers[key])
      }
    } else if (headers != null) {
      throw new InvalidArgumentError('headers must be an object or an array')
    }

    if (util.isFormDataLike(this.body)) {
      if (nodeMajor < 16 || (nodeMajor === 16 && nodeMinor < 8)) {
        throw new InvalidArgumentError('Form-Data bodies are only supported in node v16.8 and newer.')
      }

      if (!extractBody) {
        extractBody = require('../fetch/body.js').extractBody
      }

      const [bodyStream, contentType] = extractBody(body)
      if (this.contentType == null) {
        this.contentType = contentType
        this.headers += `content-type: ${contentType}\r\n`
      }
      this.body = bodyStream.stream
    } else if (util.isBlobLike(body) && this.contentType == null && body.type) {
      this.contentType = body.type
      this.headers += `content-type: ${body.type}\r\n`
    }

    util.validateHandler(handler, method, upgrade)

    this.servername = util.getServerName(this.host)

    this[kHandler] = handler

    if (channels.create.hasSubscribers) {
      channels.create.publish({ request: this })
    }
  }

  onBodySent (chunk) {
    if (this[kHandler].onBodySent) {
      try {
        this[kHandler].onBodySent(chunk)
      } catch (err) {
        this.onError(err)
      }
    }
  }

  onRequestSent () {
    if (channels.bodySent.hasSubscribers) {
      channels.bodySent.publish({ request: this })
    }
  }

  onConnect (abort) {
    assert(!this.aborted)
    assert(!this.completed)

    return this[kHandler].onConnect(abort)
  }

  onHeaders (statusCode, headers, resume, statusText) {
    assert(!this.aborted)
    assert(!this.completed)

    if (channels.headers.hasSubscribers) {
      channels.headers.publish({ request: this, response: { statusCode, headers, statusText } })
    }

    return this[kHandler].onHeaders(statusCode, headers, resume, statusText)
  }

  onData (chunk) {
    assert(!this.aborted)
    assert(!this.completed)

    return this[kHandler].onData(chunk)
  }

  onUpgrade (statusCode, headers, socket) {
    assert(!this.aborted)
    assert(!this.completed)

    return this[kHandler].onUpgrade(statusCode, headers, socket)
  }

  onComplete (trailers) {
    assert(!this.aborted)

    this.completed = true
    if (channels.trailers.hasSubscribers) {
      channels.trailers.publish({ request: this, trailers })
    }
    return this[kHandler].onComplete(trailers)
  }

  onError (error) {
    if (channels.error.hasSubscribers) {
      channels.error.publish({ request: this, error })
    }

    if (this.aborted) {
      return
    }
    this.aborted = true
    return this[kHandler].onError(error)
  }

  addHeader (key, value) {
    processHeader(this, key, value)
    return this
  }
}

function processHeader (request, key, val) {
  if (val && typeof val === 'object') {
    throw new InvalidArgumentError(`invalid ${key} header`)
  } else if (val === undefined) {
    return
  }

  if (
    request.host === null &&
    key.length === 4 &&
    key.toLowerCase() === 'host'
  ) {
    // Consumed by Client
    request.host = val
  } else if (
    request.contentLength === null &&
    key.length === 14 &&
    key.toLowerCase() === 'content-length'
  ) {
    request.contentLength = parseInt(val, 10)
    if (!Number.isFinite(request.contentLength)) {
      throw new InvalidArgumentError('invalid content-length header')
    }
  } else if (
    request.contentType === null &&
    key.length === 12 &&
    key.toLowerCase() === 'content-type'
  ) {
    request.contentType = val
    request.headers += `${key}: ${val}\r\n`
  } else if (
    key.length === 17 &&
    key.toLowerCase() === 'transfer-encoding'
  ) {
    throw new InvalidArgumentError('invalid transfer-encoding header')
  } else if (
    key.length === 10 &&
    key.toLowerCase() === 'connection'
  ) {
    throw new InvalidArgumentError('invalid connection header')
  } else if (
    key.length === 10 &&
    key.toLowerCase() === 'keep-alive'
  ) {
    throw new InvalidArgumentError('invalid keep-alive header')
  } else if (
    key.length === 7 &&
    key.toLowerCase() === 'upgrade'
  ) {
    throw new InvalidArgumentError('invalid upgrade header')
  } else if (
    key.length === 6 &&
    key.toLowerCase() === 'expect'
  ) {
    throw new NotSupportedError('expect header not supported')
  } else if (tokenRegExp.exec(key) === null) {
    throw new InvalidArgumentError('invalid header key')
  } else if (headerCharRegex.exec(val) !== null) {
    throw new InvalidArgumentError(`invalid ${key} header`)
  } else {
    request.headers += `${key}: ${val}\r\n`
  }
}

module.exports = Request
