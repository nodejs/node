'use strict'
const EventEmitter = require('node:events')
const { InvalidArgumentError } = require('../core/errors')
const { kOriginless, kUrl, kInterceptorKind } = require('../core/symbols')

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

    const orderSensitiveKinds = []

    for (const interceptor of interceptors) {
      if (interceptor == null) {
        continue
      }

      if (typeof interceptor !== 'function') {
        throw new TypeError(`invalid interceptor, expected function received ${typeof interceptor}`)
      }

      if (interceptor[kInterceptorKind] != null) {
        orderSensitiveKinds.push(interceptor[kInterceptorKind])
      }

      dispatch = interceptor(dispatch, interceptorOrigin)

      if (dispatch == null || typeof dispatch !== 'function' || dispatch.length !== 2) {
        throw new TypeError('invalid interceptor')
      }
    }

    // Several shipped interceptors are order-sensitive: they cooperate through
    // the redirect history threaded on the request context, and the cache
    // handler only receives it when the redirect interceptor is composed before
    // the cache interceptor. Composing `cache` before `redirect` silently
    // disables the cache's redirect-origin guards (cross-origin cache
    // poisoning, credential-bearing redirect replay), so fail fast instead of
    // storing an attacker-controlled or cross-principal response under a key
    // the application trusts.
    const cacheIndex = orderSensitiveKinds.indexOf('cache')
    const redirectIndex = orderSensitiveKinds.indexOf('redirect')
    if (cacheIndex !== -1 && redirectIndex !== -1 && cacheIndex < redirectIndex) {
      throw new InvalidArgumentError('the cache interceptor must be composed after the redirect interceptor; composing the cache before the redirect disables the cache\'s cross-origin and credential redirect guards (GHSA-75VM-9G8F-99J4, GHSA-MQG3-3V4R-CM32, GHSA-4W7W-5XRC-9H26, GHSA-7XG3-FPHP-28HG)')
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
