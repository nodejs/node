'use strict'

const CachePolicy = require('http-cache-semantics')

const iterableToObject = require('./iterable-to-object')

module.exports = function makePolicy (req, res) {
  const _req = {
    url: req.url,
    method: req.method,
    headers: iterableToObject(req.headers),
  }
  const _res = {
    status: res.status,
    headers: iterableToObject(res.headers),
  }

  return new CachePolicy(_req, _res, { shared: false })
}
