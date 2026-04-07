'use strict'

const Dispatcher = require('./dispatcher')
const { InvalidArgumentError } = require('../core/errors')
const { toRawHeaders } = require('../core/util')

class LegacyHandlerWrapper {
  #handler

  constructor (handler) {
    this.#handler = handler
  }

  onRequestStart (controller, context) {
    this.#handler.onConnect?.((reason) => controller.abort(reason), context)
  }

  onRequestUpgrade (controller, statusCode, headers, socket) {
    const rawHeaders = controller?.rawHeaders ?? toRawHeaders(headers ?? {})
    this.#handler.onUpgrade?.(statusCode, rawHeaders, socket)
  }

  onResponseStart (controller, statusCode, headers, statusMessage) {
    const rawHeaders = controller?.rawHeaders ?? toRawHeaders(headers ?? {})

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
    const rawTrailers = controller?.rawTrailers ?? toRawHeaders(trailers ?? {})
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
    return this.#dispatcher.dispatch(opts, Dispatcher1Wrapper.wrapHandler(handler))
  }

  close (...args) {
    return this.#dispatcher.close(...args)
  }

  destroy (...args) {
    return this.#dispatcher.destroy(...args)
  }
}

module.exports = Dispatcher1Wrapper
