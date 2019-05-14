'use strict'

const eu = encodeURIComponent
const fetch = require('npm-registry-fetch')
const figgyPudding = require('figgy-pudding')
const getStream = require('get-stream')
const validate = require('aproba')

const OrgConfig = figgyPudding({
  Promise: {default: () => Promise}
})

// From https://github.com/npm/registry/blob/master/docs/orgs/memberships.md
const cmd = module.exports = {}

class MembershipDetail {}
cmd.set = (org, user, role, opts) => {
  if (typeof role === 'object' && !opts) {
    opts = role
    role = undefined
  }
  opts = OrgConfig(opts)
  return new opts.Promise((resolve, reject) => {
    validate('SSSO|SSZO', [org, user, role, opts])
    user = user.replace(/^@?/, '')
    org = org.replace(/^@?/, '')
    fetch.json(`/-/org/${eu(org)}/user`, opts.concat({
      method: 'PUT',
      body: {user, role}
    })).then(resolve, reject)
  }).then(ret => Object.assign(new MembershipDetail(), ret))
}

cmd.rm = (org, user, opts) => {
  opts = OrgConfig(opts)
  return new opts.Promise((resolve, reject) => {
    validate('SSO', [org, user, opts])
    user = user.replace(/^@?/, '')
    org = org.replace(/^@?/, '')
    fetch(`/-/org/${eu(org)}/user`, opts.concat({
      method: 'DELETE',
      body: {user},
      ignoreBody: true
    })).then(resolve, reject)
  }).then(() => null)
}

class Roster {}
cmd.ls = (org, opts) => {
  opts = OrgConfig(opts)
  return new opts.Promise((resolve, reject) => {
    getStream.array(cmd.ls.stream(org, opts)).then(entries => {
      const obj = {}
      for (let [key, val] of entries) {
        obj[key] = val
      }
      return obj
    }).then(resolve, reject)
  }).then(ret => Object.assign(new Roster(), ret))
}

cmd.ls.stream = (org, opts) => {
  opts = OrgConfig(opts)
  validate('SO', [org, opts])
  org = org.replace(/^@?/, '')
  return fetch.json.stream(`/-/org/${eu(org)}/user`, '*', opts.concat({
    mapJson (value, [key]) {
      return [key, value]
    }
  }))
}
