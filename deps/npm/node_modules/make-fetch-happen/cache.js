'use strict'

const cacache = require('cacache')
const fetch = require('node-fetch-npm')
const pipe = require('mississippi').pipe
const ssri = require('ssri')
const through = require('mississippi').through
const to = require('mississippi').to
const url = require('url')
const stream = require('stream')

const MAX_MEM_SIZE = 5 * 1024 * 1024 // 5MB

function cacheKey (req) {
  const parsed = url.parse(req.url)
  return `make-fetch-happen:request-cache:${
    url.format({
      protocol: parsed.protocol,
      slashes: parsed.slashes,
      host: parsed.host,
      hostname: parsed.hostname,
      pathname: parsed.pathname
    })
  }`
}

// This is a cacache-based implementation of the Cache standard,
// using node-fetch.
// docs: https://developer.mozilla.org/en-US/docs/Web/API/Cache
//
module.exports = class Cache {
  constructor (path, opts) {
    this._path = path
    this.Promise = (opts && opts.Promise) || Promise
  }

  // Returns a Promise that resolves to the response associated with the first
  // matching request in the Cache object.
  match (req, opts) {
    opts = opts || {}
    const key = cacheKey(req)
    return cacache.get.info(this._path, key).then(info => {
      return info && cacache.get.hasContent(
        this._path, info.integrity, opts
      ).then(exists => exists && info)
    }).then(info => {
      if (info && info.metadata && matchDetails(req, {
        url: info.metadata.url,
        reqHeaders: new fetch.Headers(info.metadata.reqHeaders),
        resHeaders: new fetch.Headers(info.metadata.resHeaders),
        cacheIntegrity: info.integrity,
        integrity: opts && opts.integrity
      })) {
        const resHeaders = new fetch.Headers(info.metadata.resHeaders)
        addCacheHeaders(resHeaders, this._path, key, info.integrity, info.time)
        if (req.method === 'HEAD') {
          return new fetch.Response(null, {
            url: req.url,
            headers: resHeaders,
            status: 200
          })
        }
        let body
        const cachePath = this._path
        // avoid opening cache file handles until a user actually tries to
        // read from it.
        if (opts.memoize !== false && info.size > MAX_MEM_SIZE) {
          body = new stream.PassThrough()
          const realRead = body._read
          body._read = function (size) {
            body._read = realRead
            pipe(
              cacache.get.stream.byDigest(cachePath, info.integrity, {
                memoize: opts.memoize
              }),
              body,
              err => body.emit(err))
            return realRead.call(this, size)
          }
        } else {
          let readOnce = false
          // cacache is much faster at bulk reads
          body = new stream.Readable({
            read () {
              if (readOnce) return this.push(null)
              readOnce = true
              cacache.get.byDigest(cachePath, info.integrity, {
                memoize: opts.memoize
              }).then(data => {
                this.push(data)
                this.push(null)
              }, err => this.emit('error', err))
            }
          })
        }
        return this.Promise.resolve(new fetch.Response(body, {
          url: req.url,
          headers: resHeaders,
          status: 200,
          size: info.size
        }))
      }
    })
  }

  // Takes both a request and its response and adds it to the given cache.
  put (req, response, opts) {
    opts = opts || {}
    const size = response.headers.get('content-length')
    const fitInMemory = !!size && opts.memoize !== false && size < MAX_MEM_SIZE
    const ckey = cacheKey(req)
    const cacheOpts = {
      algorithms: opts.algorithms,
      metadata: {
        url: req.url,
        reqHeaders: req.headers.raw(),
        resHeaders: response.headers.raw()
      },
      size,
      memoize: fitInMemory && opts.memoize
    }
    if (req.method === 'HEAD' || response.status === 304) {
      // Update metadata without writing
      return cacache.get.info(this._path, ckey).then(info => {
        // Providing these will bypass content write
        cacheOpts.integrity = info.integrity
        addCacheHeaders(
          response.headers, this._path, ckey, info.integrity, info.time
        )
        return new this.Promise((resolve, reject) => {
          pipe(
            cacache.get.stream.byDigest(this._path, info.integrity, cacheOpts),
            cacache.put.stream(this._path, cacheKey(req), cacheOpts),
            err => err ? reject(err) : resolve(response)
          )
        })
      }).then(() => response)
    }
    let buf = []
    let bufSize = 0
    let cacheTargetStream = false
    const cachePath = this._path
    let cacheStream = to((chunk, enc, cb) => {
      if (!cacheTargetStream) {
        if (fitInMemory) {
          cacheTargetStream =
          to({highWaterMark: MAX_MEM_SIZE}, (chunk, enc, cb) => {
            buf.push(chunk)
            bufSize += chunk.length
            cb()
          }, done => {
            cacache.put(
              cachePath,
              cacheKey(req),
              Buffer.concat(buf, bufSize),
              cacheOpts
            ).then(
              () => done(),
              done
            )
          })
        } else {
          cacheTargetStream =
          cacache.put.stream(cachePath, cacheKey(req), cacheOpts)
        }
      }
      cacheTargetStream.write(chunk, enc, cb)
    }, done => {
      cacheTargetStream ? cacheTargetStream.end(done) : done()
    })
    const oldBody = response.body
    const newBody = through({highWaterMark: MAX_MEM_SIZE})
    response.body = newBody
    oldBody.once('error', err => newBody.emit('error', err))
    newBody.once('error', err => oldBody.emit('error', err))
    cacheStream.once('error', err => newBody.emit('error', err))
    pipe(oldBody, to((chunk, enc, cb) => {
      cacheStream.write(chunk, enc, () => {
        newBody.write(chunk, enc, cb)
      })
    }, done => {
      cacheStream.end(() => {
        newBody.end(() => {
          done()
        })
      })
    }), err => err && newBody.emit('error', err))
    return response
  }

  // Finds the Cache entry whose key is the request, and if found, deletes the
  // Cache entry and returns a Promise that resolves to true. If no Cache entry
  // is found, it returns false.
  'delete' (req, opts) {
    opts = opts || {}
    if (typeof opts.memoize === 'object') {
      if (opts.memoize.reset) {
        opts.memoize.reset()
      } else if (opts.memoize.clear) {
        opts.memoize.clear()
      } else {
        Object.keys(opts.memoize).forEach(k => {
          opts.memoize[k] = null
        })
      }
    }
    return cacache.rm.entry(
      this._path,
      cacheKey(req)
    // TODO - true/false
    ).then(() => false)
  }
}

function matchDetails (req, cached) {
  const reqUrl = url.parse(req.url)
  const cacheUrl = url.parse(cached.url)
  const vary = cached.resHeaders.get('Vary')
  // https://tools.ietf.org/html/rfc7234#section-4.1
  if (vary) {
    if (vary.match(/\*/)) {
      return false
    } else {
      const fieldsMatch = vary.split(/\s*,\s*/).every(field => {
        return cached.reqHeaders.get(field) === req.headers.get(field)
      })
      if (!fieldsMatch) {
        return false
      }
    }
  }
  if (cached.integrity) {
    return ssri.parse(cached.integrity).match(cached.cacheIntegrity)
  }
  reqUrl.hash = null
  cacheUrl.hash = null
  return url.format(reqUrl) === url.format(cacheUrl)
}

function addCacheHeaders (resHeaders, path, key, hash, time) {
  resHeaders.set('X-Local-Cache', encodeURIComponent(path))
  resHeaders.set('X-Local-Cache-Key', encodeURIComponent(key))
  resHeaders.set('X-Local-Cache-Hash', encodeURIComponent(hash))
  resHeaders.set('X-Local-Cache-Time', new Date(time).toUTCString())
}
