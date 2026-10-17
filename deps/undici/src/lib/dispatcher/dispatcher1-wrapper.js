'use strict'

const Dispatcher = require('./dispatcher')
const { InvalidArgumentError } = require('../core/errors')
const { toRawHeaders } = require('../core/util')
const { kOriginless, kUrl } = require('../core/symbols')

class LegacyHandlerWrapper {
  #handler

  constructor (handler) {
    this.#handler = handler
  }

  onRequestStart (controller, context) {
    this.#handler.onConnect?.((reason) => controller.abort(reason), context)
  }

  onRequestUpgrade (controller, statusCode, headers, socket) {
    const rawHeaders = Array.isArray(controller?.rawHeaders)
      ? controller.rawHeaders
      : toRawHeaders(controller?.rawHeaders ?? headers ?? {})
    this.#handler.onUpgrade?.(statusCode, rawHeaders, socket)
  }

  onResponseStart (controller, statusCode, headers, statusMessage) {
    const rawHeaders = Array.isArray(controller?.rawHeaders)
      ? controller.rawHeaders
      : toRawHeaders(controller?.rawHeaders ?? headers ?? {})

    if (this.#handler.onHeaders?.(statusCode, rawHeaders, () => controller.resume(), statusMessage) === false) {
      controller.pause()
    }
  }

  onResponseData (controller, chunk) {
    if (this.#handler.onData?.(chunk) === false) {
      controller.pause()
    }
  }

  onResponseEnd (controller, trailers) {
    const rawTrailers = Array.isArray(controller?.rawTrailers)
      ? controller.rawTrailers
      : toRawHeaders(controller?.rawTrailers ?? trailers ?? {})
    this.#handler.onComplete?.(rawTrailers)
  }

  onResponseError (_controller, err) {
    if (!this.#handler.onError) {
      throw err
    }

    this.#handler.onError(err)
  }

  onBodySent (chunk) {
    this.#handler.onBodySent?.(chunk)
  }

  onRequestSent () {
    this.#handler.onRequestSent?.()
  }

  onResponseStarted () {
    this.#handler.onResponseStarted?.()
  }
}

class Dispatcher1Wrapper extends Dispatcher {
  #dispatcher

  constructor (dispatcher) {
    super()

    if (!dispatcher || typeof dispatcher.dispatch !== 'function') {
      throw new InvalidArgumentError('Argument dispatcher must implement dispatch')
    }

    this.#dispatcher = dispatcher
    this[kUrl] = dispatcher[kUrl]
    this[kOriginless] = dispatcher[kOriginless]
  }

  static wrapHandler (handler) {
    if (!handler || typeof handler !== 'object') {
      throw new InvalidArgumentError('handler must be an object')
    }

    if (typeof handler.onRequestStart === 'function') {
      return handler
    }

    return new LegacyHandlerWrapper(handler)
  }

  dispatch (opts, handler) {
    // Legacy (v1) consumers do not support HTTP/2, so force HTTP/1.1.
    // See https://github.com/nodejs/undici/issues/4989
    if (opts.allowH2 !== false) {
      opts = { ...opts, allowH2: false }
    }

    return this.#dispatcher.dispatch(opts, Dispatcher1Wrapper.wrapHandler(handler))
  }

  // Older undici versions read WebSocket and EventSource receive limits from
  // the (legacy) global dispatcher, which is this wrapper, so report the
  // wrapped dispatcher's settings.
  get webSocketOptions () {
    return this.#dispatcher.webSocketOptions
  }

  get eventSourceOptions () {
    return this.#dispatcher.eventSourceOptions
  }

  close (...args) {
    return this.#dispatcher.close(...args)
  }

  destroy (...args) {
    return this.#dispatcher.destroy(...args)
  }
}

module.exports = Dispatcher1Wrapper
