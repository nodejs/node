'use strict'

const Minipass = require('minipass')
const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const validate = require('aproba')

const eu = encodeURIComponent
const npar = spec => {
  spec = npa(spec)
  if (!spec.registry) {
    throw new Error('`spec` must be a registry spec')
  }
  return spec
}
const mapJSON = (value, [key]) => {
  if (value === 'read') {
    return [key, 'read-only']
  } else if (value === 'write') {
    return [key, 'read-write']
  } else {
    return [key, value]
  }
}

const cmd = module.exports = {}

cmd.public = (spec, opts) => setAccess(spec, 'public', opts)
cmd.restricted = (spec, opts) => setAccess(spec, 'restricted', opts)
function setAccess (spec, access, opts = {}) {
  return Promise.resolve().then(() => {
    spec = npar(spec)
    validate('OSO', [spec, access, opts])
    const uri = `/-/package/${eu(spec.name)}/access`
    return npmFetch(uri, {
      ...opts,
      method: 'POST',
      body: { access },
      spec
    }).then(() => true)
  })
}

cmd.grant = (spec, entity, permissions, opts = {}) => {
  return Promise.resolve().then(() => {
    spec = npar(spec)
    const { scope, team } = splitEntity(entity)
    validate('OSSSO', [spec, scope, team, permissions, opts])
    if (permissions !== 'read-write' && permissions !== 'read-only') {
      throw new Error('`permissions` must be `read-write` or `read-only`. Got `' + permissions + '` instead')
    }
    const uri = `/-/team/${eu(scope)}/${eu(team)}/package`
    return npmFetch(uri, {
      ...opts,
      method: 'PUT',
      body: { package: spec.name, permissions },
      scope,
      spec,
      ignoreBody: true
    })
      .then(() => true)
  })
}

cmd.revoke = (spec, entity, opts = {}) => {
  return Promise.resolve().then(() => {
    spec = npar(spec)
    const { scope, team } = splitEntity(entity)
    validate('OSSO', [spec, scope, team, opts])
    const uri = `/-/team/${eu(scope)}/${eu(team)}/package`
    return npmFetch(uri, {
      ...opts,
      method: 'DELETE',
      body: { package: spec.name },
      scope,
      spec,
      ignoreBody: true
    })
      .then(() => true)
  })
}

cmd.lsPackages = (entity, opts) => {
  return cmd.lsPackages.stream(entity, opts)
    .collect()
    .then(data => {
      return data.reduce((acc, [key, val]) => {
        if (!acc) {
          acc = {}
        }
        acc[key] = val
        return acc
      }, null)
    })
}

cmd.lsPackages.stream = (entity, opts = {}) => {
  validate('SO|SZ', [entity, opts])
  const { scope, team } = splitEntity(entity)
  let uri
  if (team) {
    uri = `/-/team/${eu(scope)}/${eu(team)}/package`
  } else {
    uri = `/-/org/${eu(scope)}/package`
  }
  const nextOpts = {
    ...opts,
    query: { format: 'cli' },
    mapJSON
  }
  const ret = new Minipass({ objectMode: true })
  npmFetch.json.stream(uri, '*', nextOpts)
    .on('error', err => {
      if (err.code === 'E404' && !team) {
        uri = `/-/user/${eu(scope)}/package`
        npmFetch.json.stream(uri, '*', nextOpts)
          .on('error', err => ret.emit('error', err))
          .pipe(ret)
      } else {
        ret.emit('error', err)
      }
    })
    .pipe(ret)
  return ret
}

cmd.lsCollaborators = (spec, user, opts) => {
  return Promise.resolve().then(() => {
    return cmd.lsCollaborators.stream(spec, user, opts)
      .collect()
      .then(data => {
        return data.reduce((acc, [key, val]) => {
          if (!acc) {
            acc = {}
          }
          acc[key] = val
          return acc
        }, null)
      })
  })
}

cmd.lsCollaborators.stream = (spec, user, opts) => {
  if (typeof user === 'object' && !opts) {
    opts = user
    user = undefined
  } else if (!opts) {
    opts = {}
  }
  spec = npar(spec)
  validate('OSO|OZO', [spec, user, opts])
  const uri = `/-/package/${eu(spec.name)}/collaborators`
  return npmFetch.json.stream(uri, '*', {
    ...opts,
    query: { format: 'cli', user: user || undefined },
    mapJSON
  })
}

cmd.tfaRequired = (spec, opts) => setRequires2fa(spec, true, opts)
cmd.tfaNotRequired = (spec, opts) => setRequires2fa(spec, false, opts)
function setRequires2fa (spec, required, opts = {}) {
  return Promise.resolve().then(() => {
    spec = npar(spec)
    validate('OBO', [spec, required, opts])
    const uri = `/-/package/${eu(spec.name)}/access`
    return npmFetch(uri, {
      ...opts,
      method: 'POST',
      body: { publish_requires_tfa: required },
      spec,
      ignoreBody: true
    }).then(() => true)
  })
}

cmd.edit = () => {
  throw new Error('Not implemented yet')
}

function splitEntity (entity = '') {
  const [, scope, team] = entity.match(/^@?([^:]+)(?::(.*))?$/) || []
  return { scope, team }
}
