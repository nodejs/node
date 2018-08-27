'use strict'

const config = require('./config')
const fetch = require('npm-registry-fetch')

module.exports = {
  add (name, endpoint, secret, opts) {
    let type = 'package'
    if (name && name.match(/^@[^/]+$/)) {
      type = 'scope'
    }
    if (name && name[0] === '~') {
      type = 'owner'
      name = name.substr(1)
    }

    opts = config({
      method: 'POST',
      body: { type, name, endpoint, secret }
    }, opts)
    return fetch.json('/-/npm/v1/hooks/hook', opts)
  },

  rm (id, opts) {
    return fetch.json(`/-/npm/v1/hooks/hook/${encodeURIComponent(id)}`, config({
      method: 'DELETE'
    }, opts))
  },

  ls (pkg, opts) {
    return fetch.json('/-/npm/v1/hooks', config({query: pkg && {package: pkg}}, opts))
      .then(json => json.objects)
  },

  update (id, endpoint, secret, opts) {
    return fetch.json(`/-/npm/v1/hooks/hook/${encodeURIComponent(id)}`, config({
      method: 'PUT',
      body: {endpoint, secret}
    }, opts))
  }
}
