/* eslint-disable standard/no-callback-literal */

const columns = require('cli-columns')
const figgyPudding = require('figgy-pudding')
const libteam = require('libnpm/team')
const npmConfig = require('./config/figgy-config.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const usage = require('./utils/usage')

module.exports = team

team.subcommands = ['create', 'destroy', 'add', 'rm', 'ls', 'edit']

team.usage = usage(
  'team',
  'npm team create <scope:team>\n' +
  'npm team destroy <scope:team>\n' +
  'npm team add <scope:team> <user>\n' +
  'npm team rm <scope:team> <user>\n' +
  'npm team ls <scope>|<scope:team>\n' +
  'npm team edit <scope:team>'
)

const TeamConfig = figgyPudding({
  json: {},
  loglevel: {},
  parseable: {},
  silent: {}
})

function UsageError () {
  throw Object.assign(new Error(team.usage), {code: 'EUSAGE'})
}

team.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv.length === 2) {
    return cb(null, team.subcommands)
  }
  switch (argv[2]) {
    case 'ls':
    case 'create':
    case 'destroy':
    case 'add':
    case 'rm':
    case 'edit':
      return cb(null, [])
    default:
      return cb(new Error(argv[2] + ' not recognized'))
  }
}

function team ([cmd, entity = '', user = ''], cb) {
  // Entities are in the format <scope>:<team>
  otplease(npmConfig(), opts => {
    opts = TeamConfig(opts).concat({description: null})
    entity = entity.replace(/^@/, '')
    switch (cmd) {
      case 'create': return teamCreate(entity, opts)
      case 'destroy': return teamDestroy(entity, opts)
      case 'add': return teamAdd(entity, user, opts)
      case 'rm': return teamRm(entity, user, opts)
      case 'ls': {
        const match = entity.match(/[^:]+:.+/)
        if (match) {
          return teamListUsers(entity, opts)
        } else {
          return teamListTeams(entity, opts)
        }
      }
      case 'edit':
        throw new Error('`npm team edit` is not implemented yet.')
      default:
        UsageError()
    }
  }).then(
    data => cb(null, data),
    err => err.code === 'EUSAGE' ? cb(err.message) : cb(err)
  )
}

function teamCreate (entity, opts) {
  return libteam.create(entity, opts).then(() => {
    if (opts.json) {
      output(JSON.stringify({
        created: true,
        team: entity
      }))
    } else if (opts.parseable) {
      output(`${entity}\tcreated`)
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`+@${entity}`)
    }
  })
}

function teamDestroy (entity, opts) {
  return libteam.destroy(entity, opts).then(() => {
    if (opts.json) {
      output(JSON.stringify({
        deleted: true,
        team: entity
      }))
    } else if (opts.parseable) {
      output(`${entity}\tdeleted`)
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`-@${entity}`)
    }
  })
}

function teamAdd (entity, user, opts) {
  return libteam.add(user, entity, opts).then(() => {
    if (opts.json) {
      output(JSON.stringify({
        added: true,
        team: entity,
        user
      }))
    } else if (opts.parseable) {
      output(`${user}\t${entity}\tadded`)
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`${user} added to @${entity}`)
    }
  })
}

function teamRm (entity, user, opts) {
  return libteam.rm(user, entity, opts).then(() => {
    if (opts.json) {
      output(JSON.stringify({
        removed: true,
        team: entity,
        user
      }))
    } else if (opts.parseable) {
      output(`${user}\t${entity}\tremoved`)
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`${user} removed from @${entity}`)
    }
  })
}

function teamListUsers (entity, opts) {
  return libteam.lsUsers(entity, opts).then(users => {
    users = users.sort()
    if (opts.json) {
      output(JSON.stringify(users, null, 2))
    } else if (opts.parseable) {
      output(users.join('\n'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`\n@${entity} has ${users.length} user${users.length === 1 ? '' : 's'}:\n`)
      output(columns(users, {padding: 1}))
    }
  })
}

function teamListTeams (entity, opts) {
  return libteam.lsTeams(entity, opts).then(teams => {
    teams = teams.sort()
    if (opts.json) {
      output(JSON.stringify(teams, null, 2))
    } else if (opts.parseable) {
      output(teams.join('\n'))
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      output(`\n@${entity} has ${teams.length} team${teams.length === 1 ? '' : 's'}:\n`)
      output(columns(teams.map(t => `@${t}`), {padding: 1}))
    }
  })
}
