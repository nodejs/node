'use strict'

const Readable = require('./readable')
const {
  InvalidArgumentError,
  RequestAbortedError
} = require('../core/errors')
const util = require('../core/util')
const { AsyncResource } = require('async_hooks')
const { addSignal, removeSignal } = require('./abort-signal')

class RequestHandler extends AsyncResource {
  constructor (opts, callback) {
    if (!opts || typeof opts !== 'object') {
      throw new InvalidArgumentError('invalid opts')
    }

    const { signal, method, opaque, body, onInfo, responseHeaders } = opts

    try {
      if (typeof callback !== 'function') {
        throw new InvalidArgumentError('invalid callback')
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

      super('UNDICI_REQUEST')
    } catch (err) {
      if (util.isStream(body)) {
        util.destroy(body.on('error', util.nop), err)
      }
      throw err
    }

    this.responseHeaders = responseHeaders || null
    this.opaque = opaque || null
    this.callback = callback
    this.res = null
    this.abort = null
    this.body = body
    this.trailers = {}
    this.context = null
    this.onInfo = onInfo || null

    if (util.isStream(body)) {
      body.on('error', (err) => {
        this.onError(err)
      })
    }

    addSignal(this, signal)
  }

  onConnect (abort, context) {
    if (!this.callback) {
      throw new RequestAbortedError()
    }

    this.abort = abort
    this.context = context
  }

  onHeaders (statusCode, rawHeaders, resume) {
    const { callback, opaque, abort, context } = this

    if (statusCode < 200) {
      if (this.onInfo) {
        const headers = this.responseHeaders === 'raw' ? util.parseRawHeaders(rawHeaders) : util.parseHeaders(rawHeaders)
        this.onInfo({ statusCode, headers })
      }
      return
    }

    const parsedHeaders = util.parseHeaders(rawHeaders)
    const body = new Readable(resume, abort, parsedHeaders['content-type'])

    this.callback = null
    this.res = body
    const headers = this.responseHeaders === 'raw' ? util.parseRawHeaders(rawHeaders) : util.parseHeaders(rawHeaders)

    this.runInAsyncScope(callback, null, null, {
      statusCode,
      headers,
      trailers: this.trailers,
      opaque,
      body,
      context
    })
  }

  onData (chunk) {
    const { res } = this
    return res.push(chunk)
  }

  onComplete (trailers) {
    const { res } = this

    removeSignal(this)

    util.parseHeaders(trailers, this.trailers)

    res.push(null)
  }

  onError (err) {
    const { res, callback, body, opaque } = this

    removeSignal(this)

    if (callback) {
      // TODO: Does this need queueMicrotask?
      this.callback = null
      queueMicrotask(() => {
        this.runInAsyncScope(callback, null, err, { opaque })
      })
    }

    if (res) {
      this.res = null
      // Ensure all queued handlers are invoked before destroying res.
      queueMicrotask(() => {
        util.destroy(res, err)
      })
    }

    if (body) {
      this.body = null
      util.destroy(body, err)
    }
  }
}

function request (opts, callback) {
  if (callback === undefined) {
    return new Promise((resolve, reject) => {
      request.call(this, opts, (err, data) => {
        return err ? reject(err) : resolve(data)
      })
    })
  }

  try {
    this.dispatch(opts, new RequestHandler(opts, callback))
  } catch (err) {
    if (typeof callback !== 'function') {
      throw err
    }
    const opaque = opts && opts.opaque
    queueMicrotask(() => callback(err, { opaque }))
  }
}

module.exports = request
