'use strict'

const assert = require('node:assert')
const { finished } = require('node:stream')
const { AsyncResource } = require('node:async_hooks')
const { InvalidArgumentError, InvalidReturnValueError } = require('../core/errors')
const util = require('../core/util')
const { addSignal, removeSignal } = require('./abort-signal')

function noop () {}

class StreamHandler extends AsyncResource {
  constructor (opts, factory, callback) {
    if (!opts || typeof opts !== 'object') {
      throw new InvalidArgumentError('invalid opts')
    }

    const { signal, method, opaque, body, onInfo, responseHeaders } = opts

    try {
      if (typeof callback !== 'function') {
        throw new InvalidArgumentError('invalid callback')
      }

      if (typeof factory !== 'function') {
        throw new InvalidArgumentError('invalid factory')
      }

      if (signal && typeof signal.on !== 'function' && typeof signal.addEventListener !== 'function') {
        throw new InvalidArgumentError('signal must be an EventEmitter or EventTarget')
      }

      if (method === 'CONNECT') {
        throw new InvalidArgumentError('invalid method')
      }

      if (onInfo && typeof onInfo !== 'function') {
        throw new InvalidArgumentError('invalid onInfo callback')
      }

      super('UNDICI_STREAM')
    } catch (err) {
      if (util.isStream(body)) {
        util.destroy(body.on('error', noop), err)
      }
      throw err
    }

    this.responseHeaders = responseHeaders || null
    this.opaque = opaque || null
    this.factory = factory
    this.callback = callback
    this.res = null
    this.abort = null
    this.context = null
    this.controller = null
    this.trailers = null
    this.body = body
    this.onInfo = onInfo || null

    if (util.isStream(body)) {
      body.on('error', (err) => {
        this.onResponseError(this.controller, err)
      })
    }

    addSignal(this, signal)
  }

  onRequestStart (controller, context) {
    if (this.reason) {
      controller.abort(this.reason)
      return
    }

    assert(this.callback)

    this.controller = controller
    this.abort = (reason) => controller.abort(reason)
    this.context = context
  }

  onResponseStart (controller, statusCode, headers, _statusMessage) {
    const { factory, opaque, context, responseHeaders } = this

    const rawHeaders = controller?.rawHeaders
    const responseHeaderData = responseHeaders === 'raw'
      ? (Array.isArray(rawHeaders) ? util.parseRawHeaders(rawHeaders) : [])
      : headers

    if (statusCode < 200) {
      if (this.onInfo) {
        this.onInfo({ statusCode, headers: responseHeaderData })
      }
      return
    }

    this.factory = null

    if (factory === null) {
      return
    }

    const res = this.runInAsyncScope(factory, null, {
      statusCode,
      headers: responseHeaderData,
      opaque,
      context
    })

    if (
      !res ||
      typeof res.write !== 'function' ||
      typeof res.end !== 'function' ||
      typeof res.on !== 'function'
    ) {
      throw new InvalidReturnValueError('expected Writable')
    }

    // TODO: Avoid finished. It registers an unnecessary amount of listeners.
    finished(res, { readable: false }, (err) => {
      const { callback, res, opaque, trailers, abort } = this

      this.res = null
      if (err || !res?.readable) {
        util.destroy(res, err)
      }

      this.callback = null
      this.runInAsyncScope(callback, null, err || null, { opaque, trailers })

      if (err) {
        abort()
      }
    })

    res.on('drain', () => controller.resume())

    this.res = res

    const needDrain = res.writableNeedDrain !== undefined
      ? res.writableNeedDrain
      : res._writableState?.needDrain

    if (needDrain === true) {
      controller.pause()
    }
  }

  onResponseData (controller, chunk) {
    const { res } = this

    if (!res) {
      return
    }

    if (res.write(chunk) === false) {
      controller.pause()
    }
  }

  onResponseEnd (_controller, trailers) {
    const { res } = this

    removeSignal(this)

    if (!res) {
      return
    }

    if (trailers && typeof trailers === 'object') {
      this.trailers = trailers
    }

    res.end()
  }

  onResponseError (_controller, err) {
    const { res, callback, opaque, body } = this

    removeSignal(this)

    this.factory = null

    if (res) {
      this.res = null
      util.destroy(res, err)
    } else if (callback) {
      this.callback = null
      queueMicrotask(() => {
        this.runInAsyncScope(callback, null, err, { opaque })
      })
    }

    if (body) {
      this.body = null
      util.destroy(body, err)
    }
  }
}

function stream (opts, factory, callback) {
  if (callback === undefined) {
    return new Promise((resolve, reject) => {
      stream.call(this, opts, factory, (err, data) => {
        return err ? reject(err) : resolve(data)
      })
    })
  }

  try {
    const handler = new StreamHandler(opts, factory, callback)

    this.dispatch(opts, handler)
  } catch (err) {
    if (typeof callback !== 'function') {
      throw err
    }
    const opaque = opts?.opaque
    queueMicrotask(() => callback(err, { opaque }))
  }
}

module.exports = stream
