'use strict'

const fetch = require('npm-registry-fetch')
const figgyPudding = require('figgy-pudding')
const getStream = require('get-stream')
const validate = require('aproba')

const HooksConfig = figgyPudding({
  package: {},
  limit: {},
  offset: {},
  Promise: {default: () => Promise}
})

const eu = encodeURIComponent
const cmd = module.exports = {}
cmd.add = (name, endpoint, secret, opts) => {
  opts = HooksConfig(opts)
  validate('SSSO', [name, endpoint, secret, opts])
  let type = 'package'
  if (name.match(/^@[^/]+$/)) {
    type = 'scope'
  }
  if (name[0] === '~') {
    type = 'owner'
    name = name.substr(1)
  }
  return fetch.json('/-/npm/v1/hooks/hook', opts.concat({
    method: 'POST',
    body: { type, name, endpoint, secret }
  }))
}

cmd.rm = (id, opts) => {
  opts = HooksConfig(opts)
  validate('SO', [id, opts])
  return fetch.json(`/-/npm/v1/hooks/hook/${eu(id)}`, opts.concat({
    method: 'DELETE'
  }, opts)).catch(err => {
    if (err.code === 'E404') {
      return null
    } else {
      throw err
    }
  })
}

cmd.update = (id, endpoint, secret, opts) => {
  opts = HooksConfig(opts)
  validate('SSSO', [id, endpoint, secret, opts])
  return fetch.json(`/-/npm/v1/hooks/hook/${eu(id)}`, opts.concat({
    method: 'PUT',
    body: {endpoint, secret}
  }, opts))
}

cmd.find = (id, opts) => {
  opts = HooksConfig(opts)
  validate('SO', [id, opts])
  return fetch.json(`/-/npm/v1/hooks/hook/${eu(id)}`, opts)
}

cmd.ls = (opts) => {
  return getStream.array(cmd.ls.stream(opts))
}

cmd.ls.stream = (opts) => {
  opts = HooksConfig(opts)
  const {package: pkg, limit, offset} = opts
  validate('S|Z', [pkg])
  validate('N|Z', [limit])
  validate('N|Z', [offset])
  return fetch.json.stream('/-/npm/v1/hooks', 'objects.*', opts.concat({
    query: {
      package: pkg,
      limit,
      offset
    }
  }))
}
