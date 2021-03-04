const columns = require('cli-columns')
const libteam = require('libnpmteam')

const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const usageUtil = require('./utils/usage.js')

class Team {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    return usageUtil(
      'team',
      'npm team create <scope:team> [--otp <otpcode>]\n' +
      'npm team destroy <scope:team> [--otp <otpcode>]\n' +
      'npm team add <scope:team> <user> [--otp <otpcode>]\n' +
      'npm team rm <scope:team> <user> [--otp <otpcode>]\n' +
      'npm team ls <scope>|<scope:team>\n'
    )
  }

  async completion (opts) {
    const { conf: { argv: { remain: argv } } } = opts
    const subcommands = ['create', 'destroy', 'add', 'rm', 'ls']

    if (argv.length === 2)
      return subcommands

    if (subcommands.includes(argv[2]))
      return []

    throw new Error(argv[2] + ' not recognized')
  }

  exec (args, cb) {
    this.team(args).then(() => cb()).catch(cb)
  }

  async team ([cmd, entity = '', user = '']) {
    // Entities are in the format <scope>:<team>
    // XXX: "description" option to libnpmteam is used as a description of the
    // team, but in npm's options, this is a boolean meaning "show the
    // description in npm search output".  Hence its being set to null here.
    await otplease(this.npm.flatOptions, opts => {
      entity = entity.replace(/^@/, '')
      switch (cmd) {
        case 'create': return this.create(entity, opts)
        case 'destroy': return this.destroy(entity, opts)
        case 'add': return this.add(entity, user, opts)
        case 'rm': return this.rm(entity, user, opts)
        case 'ls': {
          const match = entity.match(/[^:]+:.+/)
          if (match)
            return this.listUsers(entity, opts)
          else
            return this.listTeams(entity, opts)
        }
        default:
          throw this.usage
      }
    })
  }

  async create (entity, opts) {
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

  async destroy (entity, opts) {
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

  async add (entity, user, opts) {
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

  async rm (entity, user, opts) {
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

  async listUsers (entity, opts) {
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

  async listTeams (entity, opts) {
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
}
module.exports = Team
