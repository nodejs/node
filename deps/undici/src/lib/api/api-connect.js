'use strict'

const assert = require('node:assert')
const { AsyncResource } = require('node:async_hooks')
const { InvalidArgumentError, SocketError } = require('../core/errors')
const util = require('../core/util')
const { addSignal, removeSignal } = require('./abort-signal')

class ConnectHandler extends AsyncResource {
  constructor (opts, callback) {
    if (!opts || typeof opts !== 'object') {
      throw new InvalidArgumentError('invalid opts')
    }

    if (typeof callback !== 'function') {
      throw new InvalidArgumentError('invalid callback')
    }

    const { signal, opaque, responseHeaders } = opts

    if (signal && typeof signal.on !== 'function' && typeof signal.addEventListener !== 'function') {
      throw new InvalidArgumentError('signal must be an EventEmitter or EventTarget')
    }

    super('UNDICI_CONNECT')

    this.opaque = opaque || null
    this.responseHeaders = responseHeaders || null
    this.callback = callback
    this.abort = null

    addSignal(this, signal)
  }

  onRequestStart (controller, context) {
    if (this.reason) {
      controller.abort(this.reason)
      return
    }

    assert(this.callback)

    this.abort = (reason) => controller.abort(reason)
    this.context = context
  }

  onResponseStart () {
    throw new SocketError('bad connect', null)
  }

  onRequestUpgrade (controller, statusCode, headers, socket) {
    const { callback, opaque, context } = this

    removeSignal(this)

    this.callback = null

    let responseHeaders = headers
    const rawHeaders = controller?.rawHeaders
    // Indicates is an HTTP2Session
    if (responseHeaders != null) {
      responseHeaders = this.responseHeaders === 'raw'
        ? (Array.isArray(rawHeaders) ? util.parseRawHeaders(rawHeaders) : [])
        : headers
    }

    this.runInAsyncScope(callback, null, null, {
      statusCode,
      headers: responseHeaders,
      socket,
      opaque,
      context
    })
  }

  onResponseError (_controller, err) {
    const { callback, opaque } = this

    removeSignal(this)

    if (callback) {
      this.callback = null
      queueMicrotask(() => {
        this.runInAsyncScope(callback, null, err, { opaque })
      })
    }
  }
}

function connect (opts, callback) {
  if (callback === undefined) {
    return new Promise((resolve, reject) => {
      connect.call(this, opts, (err, data) => {
        return err ? reject(err) : resolve(data)
      })
    })
  }

  try {
    const connectHandler = new ConnectHandler(opts, callback)
    const connectOptions = { ...opts, method: 'CONNECT' }
    this.dispatch(connectOptions, connectHandler)
  } catch (err) {
    if (typeof callback !== 'function') {
      throw err
    }
    const opaque = opts?.opaque
    queueMicrotask(() => callback(err, { opaque }))
  }
}

module.exports = connect
