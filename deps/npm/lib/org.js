'use strict'

const figgyPudding = require('figgy-pudding')
const liborg = require('libnpm/org')
const npmConfig = require('./config/figgy-config.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const Table = require('cli-table3')

module.exports = org

org.subcommands = ['set', 'rm', 'ls']

org.usage =
  'npm org set orgname username [developer | admin | owner]\n' +
  'npm org rm orgname username\n' +
  'npm org ls orgname'

const OrgConfig = figgyPudding({
  json: {},
  loglevel: {},
  parseable: {},
  silent: {}
})

org.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length === 2) {
    return cb(null, org.subcommands)
  }
  switch (argv[2]) {
    case 'ls':
    case 'add':
    case 'rm':
    case 'set':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

function UsageError () {
  throw Object.assign(new Error(org.usage), {code: 'EUSAGE'})
}

function org ([cmd, orgname, username, role], cb) {
  otplease(npmConfig(), opts => {
    opts = OrgConfig(opts)
    switch (cmd) {
      case 'add':
      case 'set':
        return orgSet(orgname, username, role, opts)
      case 'rm':
        return orgRm(orgname, username, opts)
      case 'ls':
        return orgList(orgname, opts)
      default:
        UsageError()
    }
  }).then(
    x => cb(null, x),
    err => err.code === 'EUSAGE' ? err.message : err
  )
}

function orgSet (org, user, role, opts) {
  return liborg.set(org, user, role, opts).then(memDeets => {
    if (opts.json) {
      output(JSON.stringify(memDeets, null, 2))
    } else if (opts.parseable) {
      output(['org', 'orgsize', 'user', 'role'].join('\t'))
      output([
        memDeets.org.name,
        memDeets.org.size,
        memDeets.user,
        memDeets.role
      ])
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`Added ${memDeets.user} as ${memDeets.role} to ${memDeets.org.name}. You now ${memDeets.org.size} member${memDeets.org.size === 1 ? '' : 's'} in this org.`)
    }
    return memDeets
  })
}

function orgRm (org, user, opts) {
  return liborg.rm(org, user, opts).then(() => {
    return liborg.ls(org, opts)
  }).then(roster => {
    user = user.replace(/^[~@]?/, '')
    org = org.replace(/^[~@]?/, '')
    const userCount = Object.keys(roster).length
    if (opts.json) {
      output(JSON.stringify({
        user,
        org,
        userCount,
        deleted: true
      }))
    } else if (opts.parseable) {
      output(['user', 'org', 'userCount', 'deleted'].join('\t'))
      output([user, org, userCount, true].join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`Successfully removed ${user} from ${org}. You now have ${userCount} member${userCount === 1 ? '' : 's'} in this org.`)
    }
  })
}

function orgList (org, opts) {
  return liborg.ls(org, opts).then(roster => {
    if (opts.json) {
      output(JSON.stringify(roster, null, 2))
    } else if (opts.parseable) {
      output(['user', 'role'].join('\t'))
      Object.keys(roster).forEach(user => {
        output([user, roster[user]].join('\t'))
      })
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      const table = new Table({head: ['user', 'role']})
      Object.keys(roster).sort().forEach(user => {
        table.push([user, roster[user]])
      })
      output(table.toString())
    }
  })
}
