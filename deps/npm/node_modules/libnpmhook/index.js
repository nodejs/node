'use strict'

const fetch = require('npm-registry-fetch')
const validate = require('aproba')

const eu = encodeURIComponent
const cmd = module.exports = {}
cmd.add = (name, endpoint, secret, opts = {}) => {
  validate('SSSO', [name, endpoint, secret, opts])
  let type = 'package'
  if (name.match(/^@[^/]+$/)) {
    type = 'scope'
  }
  if (name[0] === '~') {
    type = 'owner'
    name = name.substr(1)
  }
  return fetch.json('/-/npm/v1/hooks/hook', {
    ...opts,
    method: 'POST',
    body: { type, name, endpoint, secret }
  })
}

cmd.rm = (id, opts = {}) => {
  validate('SO', [id, opts])
  return fetch.json(`/-/npm/v1/hooks/hook/${eu(id)}`, {
    ...opts,
    method: 'DELETE'
  }).catch(err => {
    if (err.code === 'E404') {
      return null
    } else {
      throw err
    }
  })
}

cmd.update = (id, endpoint, secret, opts = {}) => {
  validate('SSSO', [id, endpoint, secret, opts])
  return fetch.json(`/-/npm/v1/hooks/hook/${eu(id)}`, {
    ...opts,
    method: 'PUT',
    body: {endpoint, secret}
  })
}

cmd.find = (id, opts = {}) => {
  validate('SO', [id, opts])
  return fetch.json(`/-/npm/v1/hooks/hook/${eu(id)}`, opts)
}

cmd.ls = (opts = {}) => {
  return cmd.ls.stream(opts).collect()
}

cmd.ls.stream = (opts = {}) => {
  const { package: pkg, limit, offset } = opts
  validate('S|Z', [pkg])
  validate('N|Z', [limit])
  validate('N|Z', [offset])
  return fetch.json.stream('/-/npm/v1/hooks', 'objects.*', {
    ...opts,
    query: {
      package: pkg,
      limit,
      offset
    }
  })
}
