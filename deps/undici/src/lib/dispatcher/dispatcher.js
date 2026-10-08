'use strict'
const EventEmitter = require('node:events')
const { kOriginless, kUrl } = require('../core/symbols')

class Dispatcher extends EventEmitter {
  dispatch () {
    throw new Error('not implemented')
  }

  close () {
    throw new Error('not implemented')
  }

  destroy () {
    throw new Error('not implemented')
  }

  compose (...args) {
    // So we handle [interceptor1, interceptor2] or interceptor1, interceptor2, ...
    const interceptors = Array.isArray(args[0]) ? args[0] : args
    // null disables origin-dependent interceptors; undefined uses opts.origin.
    const interceptorOrigin = this[kOriginless] === true
      ? null
      : this[kUrl]?.origin
    let dispatch = this.dispatch.bind(this)

    for (const interceptor of interceptors) {
      if (interceptor == null) {
        continue
      }

      if (typeof interceptor !== 'function') {
        throw new TypeError(`invalid interceptor, expected function received ${typeof interceptor}`)
      }

      dispatch = interceptor(dispatch, interceptorOrigin)

      if (dispatch == null || typeof dispatch !== 'function' || dispatch.length !== 2) {
        throw new TypeError('invalid interceptor')
      }
    }

    const originalDispatch = dispatch
    const self = this
    dispatch = function (opts, handler) {
      if (opts && typeof opts === 'object' && !opts.origin && self[kUrl]) {
        opts = Object.assign({}, opts, { origin: self[kUrl].origin })
      }
      return originalDispatch(opts, handler)
    }

    return new Proxy(this, {
      get: (target, key) => key === 'dispatch' ? dispatch : target[key]
    })
  }
}

module.exports = Dispatcher
