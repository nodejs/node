const columns = require('cli-columns')
const libteam = require('libnpmteam')

const npm = require('./npm.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const usageUtil = require('./utils/usage')

const subcommands = ['create', 'destroy', 'add', 'rm', 'ls']

const usage = usageUtil(
  'team',
  'npm team create <scope:team> [--otp <otpcode>]\n' +
  'npm team destroy <scope:team> [--otp <otpcode>]\n' +
  'npm team add <scope:team> <user> [--otp <otpcode>]\n' +
  'npm team rm <scope:team> <user> [--otp <otpcode>]\n' +
  'npm team ls <scope>|<scope:team>\n'
)

const completion = async (opts) => {
  const { conf: { argv: { remain: argv } } } = opts
  if (argv.length === 2)
    return subcommands

  switch (argv[2]) {
    case 'ls':
    case 'create':
    case 'destroy':
    case 'add':
    case 'rm':
      return []
    default:
      throw new Error(argv[2] + ' not recognized')
  }
}

const cmd = (args, cb) => team(args).then(() => cb()).catch(cb)

const team = async ([cmd, entity = '', user = '']) => {
  // Entities are in the format <scope>:<team>
  // XXX: "description" option to libnpmteam is used as a description of the
  // team, but in npm's options, this is a boolean meaning "show the
  // description in npm search output".  Hence its being set to null here.
  await otplease(npm.flatOptions, opts => {
    entity = entity.replace(/^@/, '')
    switch (cmd) {
      case 'create': return teamCreate(entity, opts)
      case 'destroy': return teamDestroy(entity, opts)
      case 'add': return teamAdd(entity, user, opts)
      case 'rm': return teamRm(entity, user, opts)
      case 'ls': {
        const match = entity.match(/[^:]+:.+/)
        if (match)
          return teamListUsers(entity, opts)
        else
          return teamListTeams(entity, opts)
      }
      default:
        throw usage
    }
  })
}

const teamCreate = async (entity, opts) => {
  await libteam.create(entity, opts)
  if (opts.json) {
    output(JSON.stringify({
      created: true,
      team: entity,
    }))
  } else if (opts.parseable)
    output(`${entity}\tcreated`)
  else if (!opts.silent && opts.loglevel !== 'silent')
    output(`+@${entity}`)
}

const teamDestroy = async (entity, opts) => {
  await libteam.destroy(entity, opts)
  if (opts.json) {
    output(JSON.stringify({
      deleted: true,
      team: entity,
    }))
  } else if (opts.parseable)
    output(`${entity}\tdeleted`)
  else if (!opts.silent && opts.loglevel !== 'silent')
    output(`-@${entity}`)
}

const teamAdd = async (entity, user, opts) => {
  await libteam.add(user, entity, opts)
  if (opts.json) {
    output(JSON.stringify({
      added: true,
      team: entity,
      user,
    }))
  } else if (opts.parseable)
    output(`${user}\t${entity}\tadded`)
  else if (!opts.silent && opts.loglevel !== 'silent')
    output(`${user} added to @${entity}`)
}

const teamRm = async (entity, user, opts) => {
  await libteam.rm(user, entity, opts)
  if (opts.json) {
    output(JSON.stringify({
      removed: true,
      team: entity,
      user,
    }))
  } else if (opts.parseable)
    output(`${user}\t${entity}\tremoved`)
  else if (!opts.silent && opts.loglevel !== 'silent')
    output(`${user} removed from @${entity}`)
}

const teamListUsers = async (entity, opts) => {
  const users = (await libteam.lsUsers(entity, opts)).sort()
  if (opts.json)
    output(JSON.stringify(users, null, 2))
  else if (opts.parseable)
    output(users.join('\n'))
  else if (!opts.silent && opts.loglevel !== 'silent') {
    const plural = users.length === 1 ? '' : 's'
    const more = users.length === 0 ? '' : ':\n'
    output(`\n@${entity} has ${users.length} user${plural}${more}`)
    output(columns(users, { padding: 1 }))
  }
}

const teamListTeams = async (entity, opts) => {
  const teams = (await libteam.lsTeams(entity, opts)).sort()
  if (opts.json)
    output(JSON.stringify(teams, null, 2))
  else if (opts.parseable)
    output(teams.join('\n'))
  else if (!opts.silent && opts.loglevel !== 'silent') {
    const plural = teams.length === 1 ? '' : 's'
    const more = teams.length === 0 ? '' : ':\n'
    output(`\n@${entity} has ${teams.length} team${plural}${more}`)
    output(columns(teams.map(t => `@${t}`), { padding: 1 }))
  }
}

module.exports = Object.assign(cmd, { completion, usage })
