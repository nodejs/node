'use strict'

const {
  Readable,
  Duplex,
  PassThrough
} = require('node:stream')
const assert = require('node:assert')
const { AsyncResource } = require('node:async_hooks')
const {
  InvalidArgumentError,
  InvalidReturnValueError,
  RequestAbortedError
} = require('../core/errors')
const util = require('../core/util')
const { addSignal, removeSignal } = require('./abort-signal')

function noop () {}

const kResume = Symbol('resume')

class PipelineRequest extends Readable {
  constructor () {
    super({ autoDestroy: true })

    this[kResume] = null
  }

  _read () {
    const { [kResume]: resume } = this

    if (resume) {
      this[kResume] = null
      resume()
    }
  }

  _destroy (err, callback) {
    this._read()

    callback(err)
  }
}

class PipelineResponse extends Readable {
  constructor (resume) {
    super({ autoDestroy: true })
    this[kResume] = resume
  }

  _read () {
    this[kResume]()
  }

  _destroy (err, callback) {
    if (!err && !this._readableState.endEmitted) {
      err = new RequestAbortedError()
    }

    callback(err)
  }
}

class PipelineHandler extends AsyncResource {
  constructor (opts, handler) {
    if (!opts || typeof opts !== 'object') {
      throw new InvalidArgumentError('invalid opts')
    }

    if (typeof handler !== 'function') {
      throw new InvalidArgumentError('invalid handler')
    }

    const { signal, method, opaque, onInfo, responseHeaders } = opts

    if (signal && typeof signal.on !== 'function' && typeof signal.addEventListener !== 'function') {
      throw new InvalidArgumentError('signal must be an EventEmitter or EventTarget')
    }

    if (method === 'CONNECT') {
      throw new InvalidArgumentError('invalid method')
    }

    if (onInfo && typeof onInfo !== 'function') {
      throw new InvalidArgumentError('invalid onInfo callback')
    }

    super('UNDICI_PIPELINE')

    this.opaque = opaque || null
    this.responseHeaders = responseHeaders || null
    this.handler = handler
    this.abort = null
    this.context = null
    this.onInfo = onInfo || null

    this.req = new PipelineRequest().on('error', noop)

    this.ret = new Duplex({
      readableObjectMode: opts.objectMode,
      autoDestroy: true,
      read: () => {
        const { body } = this

        if (body?.resume) {
          body.resume()
        }
      },
      write: (chunk, encoding, callback) => {
        const { req } = this

        if (req.push(chunk, encoding) || req._readableState.destroyed) {
          callback()
        } else {
          req[kResume] = callback
        }
      },
      destroy: (err, callback) => {
        const { body, req, res, ret, abort } = this

        if (!err && !ret._readableState.endEmitted) {
          err = new RequestAbortedError()
        }

        if (abort && err) {
          abort()
        }

        util.destroy(body, err)
        util.destroy(req, err)
        util.destroy(res, err)

        removeSignal(this)

        callback(err)
      }
    }).on('prefinish', () => {
      const { req } = this

      // Node < 15 does not call _final in same tick.
      req.push(null)
    })

    this.res = null

    addSignal(this, signal)
  }

  onRequestStart (controller, context) {
    const { res } = this

    if (this.reason) {
      controller.abort(this.reason)
      return
    }

    assert(!res, 'pipeline cannot be retried')

    this.abort = (reason) => controller.abort(reason)
    this.context = context
  }

  onResponseStart (controller, statusCode, headers, _statusMessage) {
    const { opaque, handler, context } = this

    if (statusCode < 200) {
      if (this.onInfo) {
        const rawHeaders = controller?.rawHeaders
        const responseHeaders = this.responseHeaders === 'raw'
          ? (Array.isArray(rawHeaders) ? util.parseRawHeaders(rawHeaders) : [])
          : headers
        this.onInfo({ statusCode, headers: responseHeaders })
      }
      return
    }

    this.res = new PipelineResponse(() => controller.resume())

    let body
    try {
      this.handler = null
      const rawHeaders = controller?.rawHeaders
      const responseHeaders = this.responseHeaders === 'raw'
        ? (Array.isArray(rawHeaders) ? util.parseRawHeaders(rawHeaders) : [])
        : headers
      body = this.runInAsyncScope(handler, null, {
        statusCode,
        headers: responseHeaders,
        opaque,
        body: this.res,
        context
      })
    } catch (err) {
      this.res.on('error', noop)
      throw err
    }

    if (!body || typeof body.on !== 'function') {
      throw new InvalidReturnValueError('expected Readable')
    }

    body
      .on('data', (chunk) => {
        const { ret, body } = this

        if (!ret.push(chunk) && body.pause) {
          body.pause()
        }
      })
      .on('error', (err) => {
        const { ret } = this

        util.destroy(ret, err)
      })
      .on('end', () => {
        const { ret } = this

        ret.push(null)
      })
      .on('close', () => {
        const { ret } = this

        if (!ret._readableState.ended) {
          util.destroy(ret, new RequestAbortedError())
        }
      })

    this.body = body
  }

  onResponseData (controller, chunk) {
    const { res } = this

    if (res.push(chunk) === false) {
      controller.pause()
    }
  }

  onResponseEnd (_controller, _trailers) {
    const { res } = this
    res.push(null)
  }

  onResponseError (_controller, err) {
    const { ret } = this
    this.handler = null
    util.destroy(ret, err)
  }
}

function pipeline (opts, handler) {
  try {
    const pipelineHandler = new PipelineHandler(opts, handler)
    this.dispatch({ ...opts, body: pipelineHandler.req }, pipelineHandler)
    return pipelineHandler.ret
  } catch (err) {
    return new PassThrough().destroy(err)
  }
}

module.exports = pipeline
