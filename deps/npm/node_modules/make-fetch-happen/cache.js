'use strict'

const fetch = require('minipass-fetch')
const cacache = require('cacache')
const ssri = require('ssri')
const url = require('url')

const Minipass = require('minipass')
const MinipassFlush = require('minipass-flush')
const MinipassCollect = require('minipass-collect')
const MinipassPipeline = require('minipass-pipeline')

const MAX_MEM_SIZE = 5 * 1024 * 1024 // 5MB

function cacheKey (req) {
  const parsed = new url.URL(req.url)
  return `make-fetch-happen:request-cache:${
    url.format({
      protocol: parsed.protocol,
      slashes: true,
      port: parsed.port,
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
        const cachePath = this._path
        // avoid opening cache file handles until a user actually tries to
        // read from it.
        const body = new Minipass()
        const fitInMemory = info.size < MAX_MEM_SIZE
        const removeOnResume = () => body.removeListener('resume', onResume)
        const onResume =
          opts.memoize !== false && fitInMemory
            ? () => {
              const c = cacache.get.stream.byDigest(cachePath, info.integrity, {
                memoize: opts.memoize
              })
              c.on('error', /* istanbul ignore next */ err => {
                body.emit('error', err)
              })
              c.pipe(body)
            }
            : () => {
              removeOnResume()
              cacache.get.byDigest(cachePath, info.integrity, {
                memoize: opts.memoize
              })
                .then(data => body.end(data))
                .catch(/* istanbul ignore next */ err => {
                  body.emit('error', err)
                })
            }
        body.once('resume', onResume)
        body.once('end', () => removeOnResume)
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

        return new MinipassPipeline(
          cacache.get.stream.byDigest(this._path, info.integrity, cacheOpts),
          cacache.put.stream(this._path, ckey, cacheOpts)
        ).promise().then(() => {
          return response
        })
      })
    }
    const oldBody = response.body
    // the flush is the last thing in the pipeline.  Build the pipeline
    // back-to-front so we don't consume the data before we use it!
    // We unshift in either a tee-stream to the cache put stream,
    // or a collecter that dumps it to cache in one go, then the
    // old body to bring in the data.
    const newBody = new MinipassPipeline(new MinipassFlush({
      flush () {
        return cacheWritePromise
      }
    }))

    let cacheWriteResolve, cacheWriteReject
    const cacheWritePromise = new Promise((resolve, reject) => {
      cacheWriteResolve = resolve
      cacheWriteReject = reject
    })
    const cachePath = this._path

    if (fitInMemory) {
      const collecter = new MinipassCollect.PassThrough()
      collecter.on('collect', data => {
        cacache.put(
          cachePath,
          ckey,
          data,
          cacheOpts
        ).then(cacheWriteResolve, cacheWriteReject)
      })
      newBody.unshift(collecter)
    } else {
      const tee = new Minipass()
      const cacheStream = cacache.put.stream(
        cachePath,
        ckey,
        cacheOpts
      )
      tee.pipe(cacheStream)
      cacheStream.promise().then(cacheWriteResolve, cacheWriteReject)
      newBody.unshift(tee)
    }

    newBody.unshift(oldBody)
    return Promise.resolve(new fetch.Response(newBody, response))
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
  const reqUrl = new url.URL(req.url)
  const cacheUrl = new url.URL(cached.url)
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
