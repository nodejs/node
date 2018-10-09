'use strict'
const common = require('./common-tap.js')
const Bluebird = require('bluebird')
const log = require('npmlog')

const http = require('http')
const EventEmitter = require('events')
// See mock-registry.md for details

class FakeRegistry extends EventEmitter {
  constructor (opts) {
    if (!opts) opts = {}
    super(opts)
    this.mocks = {}
    this.port = opts.port || common.port
    this.registry = 'http://localhost:' + this.port
    this.server = http.createServer()
    if (!opts.keepNodeAlive) this.server.unref()
    this.server.on('request', (req, res) => {
      if (this.mocks[req.method] && this.mocks[req.method][req.url]) {
        this.mocks[req.method][req.url](req, res)
      } else {
        res.statusCode = 404
        res.end(JSON.stringify({error: 'not found'}))
      }
      log.http('fake-registry', res.statusCode || 'unknown', '→', req.method, req.url)
    })
    this._error = err => {
      log.silly('fake-registry', err)
      this.emit('error', err)
    }
    this._addErrorHandler()
  }
  reset () {
    this.mocks = {}
    return this
  }
  close () {
    this.reset()
    this._removeErrorHandler()
    return new Promise((resolve, reject) => {
      this.server.once('error', reject)
      this.server.once('close', () => {
        this.removeListener('error', reject)
        resolve(this)
      })
      this.server.close()
    })
  }
  _addErrorHandler () {
    this.server.on('error', this._error)
  }
  _removeErrorHandler () {
    if (!this._error) return
    this.server.removeListener('error', this._error)
  }
  listen (cb) {
    this._removeErrorHandler()
    return this._findPort(this.port).then(port => {
      this._addErrorHandler()
      this.port = port
      this.registry = 'http://localhost:' + port
      common.port = this.port
      common.registry = this.registry
      return this
    }).asCallback(cb)
  }
  _findPort (port) {
    return new Bluebird((resolve, reject) => {
      let onListening
      const onError = err => {
        this.server.removeListener('listening', onListening)
        if (err.code === 'EADDRINUSE') {
          return resolve(this._findPort(++port))
        } else {
          return reject(err)
        }
      }
      onListening = () => {
        this.server.removeListener('error', onError)
        resolve(port)
      }
      this.server.once('error', onError)
      this.server.once('listening', onListening)
      this.server.listen(port)
    })
  }

  mock (method, url, respondWith) {
    log.http('fake-registry', 'mock', method, url, respondWith)
    if (!this.mocks[method]) this.mocks[method] = {}
    if (typeof respondWith === 'function') {
      this.mocks[method][url] = respondWith
    } else if (Array.isArray(respondWith)) {
      const [status, body] = respondWith
      this.mocks[method][url] = (req, res) => {
        res.statusCode = status
        if (typeof body === 'object') {
          res.end(JSON.stringify(body))
        } else {
          res.end(String(body))
        }
      }
    } else {
      throw new Error('Invalid args, expected: mr.mock(method, url, [status, body])')
    }
    return this
  }

  // compat
  done () {
    this.reset()
  }
  filteringRequestBody () {
    return this
  }
  post (url, matchBody) {
    return this._createReply('POST', url)
  }
  get (url) {
    return this._createReply('GET', url)
  }
  put (url, matchBody) {
    return this._createReply('PUT', url)
  }
  delete (url) {
    return this._createReply('DELETE', url)
  }
  _createReply (method, url) {
    const mr = this
    return {
      twice: function () { return this },
      reply: function (status, responseBody) {
        mr.mock(method, url, [status, responseBody])
        return mr
      }
    }
  }
}

module.exports = new FakeRegistry()
module.exports.FakeRegistry = FakeRegistry
module.exports.compat = function (opts, cb) {
  if (arguments.length === 1 && typeof opts === 'function') {
    cb = opts
    opts = {}
  }
  return new FakeRegistry(Object.assign({keepNodeAlive: true}, opts || {})).listen(cb)
}
