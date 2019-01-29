'use strict'

const eu = encodeURIComponent
const figgyPudding = require('figgy-pudding')
const getStream = require('get-stream')
const npmFetch = require('npm-registry-fetch')
const validate = require('aproba')

const TeamConfig = figgyPudding({
  description: {},
  Promise: {default: () => Promise}
})

const cmd = module.exports = {}

cmd.create = (entity, opts) => {
  opts = TeamConfig(opts)
  return pwrap(opts, () => {
    const {scope, team} = splitEntity(entity)
    validate('SSO', [scope, team, opts])
    return npmFetch.json(`/-/org/${eu(scope)}/team`, opts.concat({
      method: 'PUT',
      scope,
      body: {name: team, description: opts.description}
    }))
  })
}

cmd.destroy = (entity, opts) => {
  opts = TeamConfig(opts)
  return pwrap(opts, () => {
    const {scope, team} = splitEntity(entity)
    validate('SSO', [scope, team, opts])
    return npmFetch.json(`/-/team/${eu(scope)}/${eu(team)}`, opts.concat({
      method: 'DELETE',
      scope
    }))
  })
}

cmd.add = (user, entity, opts) => {
  opts = TeamConfig(opts)
  return pwrap(opts, () => {
    const {scope, team} = splitEntity(entity)
    validate('SSO', [scope, team, opts])
    return npmFetch.json(`/-/team/${eu(scope)}/${eu(team)}/user`, opts.concat({
      method: 'PUT',
      scope,
      body: {user}
    }))
  })
}

cmd.rm = (user, entity, opts) => {
  opts = TeamConfig(opts)
  return pwrap(opts, () => {
    const {scope, team} = splitEntity(entity)
    validate('SSO', [scope, team, opts])
    return npmFetch.json(`/-/team/${eu(scope)}/${eu(team)}/user`, opts.concat({
      method: 'DELETE',
      scope,
      body: {user}
    }))
  })
}

cmd.lsTeams = (scope, opts) => {
  opts = TeamConfig(opts)
  return pwrap(opts, () => getStream.array(cmd.lsTeams.stream(scope, opts)))
}
cmd.lsTeams.stream = (scope, opts) => {
  opts = TeamConfig(opts)
  validate('SO', [scope, opts])
  return npmFetch.json.stream(`/-/org/${eu(scope)}/team`, '.*', opts.concat({
    query: {format: 'cli'}
  }))
}

cmd.lsUsers = (entity, opts) => {
  opts = TeamConfig(opts)
  return pwrap(opts, () => getStream.array(cmd.lsUsers.stream(entity, opts)))
}
cmd.lsUsers.stream = (entity, opts) => {
  opts = TeamConfig(opts)
  const {scope, team} = splitEntity(entity)
  validate('SSO', [scope, team, opts])
  const uri = `/-/team/${eu(scope)}/${eu(team)}/user`
  return npmFetch.json.stream(uri, '.*', opts.concat({
    query: {format: 'cli'}
  }))
}

cmd.edit = () => {
  throw new Error('edit is not implemented yet')
}

function splitEntity (entity = '') {
  let [, scope, team] = entity.match(/^@?([^:]+):(.*)$/) || []
  return {scope, team}
}

function pwrap (opts, fn) {
  return new opts.Promise((resolve, reject) => {
    fn().then(resolve, reject)
  })
}
