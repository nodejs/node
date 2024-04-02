// put javascript in here
'use strict'

const Parser = require('jsonparse')
const Minipass = require('minipass')

class JSONStreamError extends Error {
  constructor (err, caller) {
    super(err.message)
    Error.captureStackTrace(this, caller || this.constructor)
  }
  get name () {
    return 'JSONStreamError'
  }
  set name (n) {}
}

const check = (x, y) =>
  typeof x === 'string' ? String(y) === x
  : x && typeof x.test === 'function' ? x.test(y)
  : typeof x === 'boolean' || typeof x === 'object' ? x
  : typeof x === 'function' ? x(y)
  : false

const _parser = Symbol('_parser')
const _onValue = Symbol('_onValue')
const _onTokenOriginal = Symbol('_onTokenOriginal')
const _onToken = Symbol('_onToken')
const _onError = Symbol('_onError')
const _count = Symbol('_count')
const _path = Symbol('_path')
const _map = Symbol('_map')
const _root = Symbol('_root')
const _header = Symbol('_header')
const _footer = Symbol('_footer')
const _setHeaderFooter = Symbol('_setHeaderFooter')
const _ending = Symbol('_ending')

class JSONStream extends Minipass {
  constructor (opts = {}) {
    super({
      ...opts,
      objectMode: true,
    })

    this[_ending] = false
    const parser = this[_parser] = new Parser()
    parser.onValue = value => this[_onValue](value)
    this[_onTokenOriginal] = parser.onToken
    parser.onToken = (token, value) => this[_onToken](token, value)
    parser.onError = er => this[_onError](er)

    this[_count] = 0
    this[_path] = typeof opts.path === 'string'
      ? opts.path.split('.').map(e =>
          e === '$*' ? { emitKey: true }
          : e === '*' ? true
          : e === '' ? { recurse: true }
          : e)
      : Array.isArray(opts.path) && opts.path.length ? opts.path
      : null

    this[_map] = typeof opts.map === 'function' ? opts.map : null
    this[_root] = null
    this[_header] = null
    this[_footer] = null
    this[_count] = 0
  }

  [_setHeaderFooter] (key, value) {
    // header has not been emitted yet
    if (this[_header] !== false) {
      this[_header] = this[_header] || {}
      this[_header][key] = value
    }

    // footer has not been emitted yet but header has
    if (this[_footer] !== false && this[_header] === false) {
      this[_footer] = this[_footer] || {}
      this[_footer][key] = value
    }
  }

  [_onError] (er) {
    // error will always happen during a write() call.
    const caller = this[_ending] ? this.end : this.write
    this[_ending] = false
    return this.emit('error', new JSONStreamError(er, caller))
  }

  [_onToken] (token, value) {
    const parser = this[_parser]
    this[_onTokenOriginal].call(parser, token, value)
    if (parser.stack.length === 0) {
      if (this[_root]) {
        const root = this[_root]
        if (!this[_path])
          super.write(root)
        this[_root] = null
        this[_count] = 0
      }
    }
  }

  [_onValue] (value) {
    const parser = this[_parser]
    // the LAST onValue encountered is the root object.
    // just overwrite it each time.
    this[_root] = value

    if(!this[_path]) return

    let i = 0 // iterates on path
    let j  = 0 // iterates on stack
    let emitKey = false
    let emitPath = false
    while (i < this[_path].length) {
      const key = this[_path][i]
      j++

      if (key && !key.recurse) {
        const c = (j === parser.stack.length) ? parser : parser.stack[j]
        if (!c) return
        if (!check(key, c.key)) {
          this[_setHeaderFooter](c.key, value)
          return
        }
        emitKey = !!key.emitKey;
        emitPath = !!key.emitPath;
        i++
      } else {
        i++
        if (i >= this[_path].length)
          return
        const nextKey = this[_path][i]
        if (!nextKey)
          return
        while (true) {
          const c = (j === parser.stack.length) ? parser : parser.stack[j]
          if (!c) return
          if (check(nextKey, c.key)) {
            i++
            if (!Object.isFrozen(parser.stack[j]))
              parser.stack[j].value = null
            break
          } else {
            this[_setHeaderFooter](c.key, value)
          }
          j++
        }
      }
    }

    // emit header
    if (this[_header]) {
      const header = this[_header]
      this[_header] = false
      this.emit('header', header)
    }
    if (j !== parser.stack.length) return

    this[_count] ++
    const actualPath = parser.stack.slice(1)
      .map(e => e.key).concat([parser.key])
    if (value !== null && value !== undefined) {
      const data = this[_map] ? this[_map](value, actualPath) : value
      if (data !== null && data !== undefined) {
        const emit = emitKey || emitPath ? { value: data } : data
        if (emitKey)
          emit.key = parser.key
        if (emitPath)
          emit.path = actualPath
        super.write(emit)
      }
    }

    if (parser.value)
      delete parser.value[parser.key]

    for (const k of parser.stack) {
      k.value = null
    }
  }

  write (chunk, encoding, cb) {
    if (typeof encoding === 'function')
      cb = encoding, encoding = null
    if (typeof chunk === 'string')
      chunk = Buffer.from(chunk, encoding)
    else if (!Buffer.isBuffer(chunk))
      return this.emit('error', new TypeError(
        'Can only parse JSON from string or buffer input'))
    this[_parser].write(chunk)
    if (cb)
      cb()
    return this.flowing
  }

  end (chunk, encoding, cb) {
    this[_ending] = true
    if (typeof encoding === 'function')
      cb = encoding, encoding = null
    if (typeof chunk === 'function')
      cb = chunk, chunk = null
    if (chunk)
      this.write(chunk, encoding)
    if (cb)
      this.once('end', cb)

    const h = this[_header]
    this[_header] = null
    const f = this[_footer]
    this[_footer] = null
    if (h)
      this.emit('header', h)
    if (f)
      this.emit('footer', f)
    return super.end()
  }

  static get JSONStreamError () { return JSONStreamError }
  static parse (path, map) {
    return new JSONStream({path, map})
  }
}

module.exports = JSONStream
