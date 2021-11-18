const liborg = require('libnpmorg')
const otplease = require('../utils/otplease.js')
const Table = require('cli-table3')
const BaseCommand = require('../base-command.js')

class Org extends BaseCommand {
  static description = 'Manage orgs'
  static name = 'org'
  static usage = [
    'set orgname username [developer | admin | owner]',
    'rm orgname username',
    'ls orgname [<username>]',
  ]

  static params = ['registry', 'otp', 'json', 'parseable']

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2) {
      return ['set', 'rm', 'ls']
    }

    switch (argv[2]) {
      case 'ls':
      case 'add':
      case 'rm':
      case 'set':
        return []
      default:
        throw new Error(argv[2] + ' not recognized')
    }
  }

  async exec ([cmd, orgname, username, role], cb) {
    return otplease(this.npm.flatOptions, opts => {
      switch (cmd) {
        case 'add':
        case 'set':
          return this.set(orgname, username, role, opts)
        case 'rm':
          return this.rm(orgname, username, opts)
        case 'ls':
          return this.ls(orgname, username, opts)
        default:
          throw this.usageError()
      }
    })
  }

  set (org, user, role, opts) {
    role = role || 'developer'
    if (!org) {
      throw new Error('First argument `orgname` is required.')
    }

    if (!user) {
      throw new Error('Second argument `username` is required.')
    }

    if (!['owner', 'admin', 'developer'].find(x => x === role)) {
      throw new Error(
        /* eslint-disable-next-line max-len */
        'Third argument `role` must be one of `owner`, `admin`, or `developer`, with `developer` being the default value if omitted.'
      )
    }

    return liborg.set(org, user, role, opts).then(memDeets => {
      if (opts.json) {
        this.npm.output(JSON.stringify(memDeets, null, 2))
      } else if (opts.parseable) {
        this.npm.output(['org', 'orgsize', 'user', 'role'].join('\t'))
        this.npm.output(
          [memDeets.org.name, memDeets.org.size, memDeets.user, memDeets.role].join('\t')
        )
      } else if (!opts.silent && opts.loglevel !== 'silent') {
        this.npm.output(
          `Added ${memDeets.user} as ${memDeets.role} to ${memDeets.org.name}. You now have ${
            memDeets.org.size
          } member${memDeets.org.size === 1 ? '' : 's'} in this org.`
        )
      }

      return memDeets
    })
  }

  rm (org, user, opts) {
    if (!org) {
      throw new Error('First argument `orgname` is required.')
    }

    if (!user) {
      throw new Error('Second argument `username` is required.')
    }

    return liborg
      .rm(org, user, opts)
      .then(() => {
        return liborg.ls(org, opts)
      })
      .then(roster => {
        user = user.replace(/^[~@]?/, '')
        org = org.replace(/^[~@]?/, '')
        const userCount = Object.keys(roster).length
        if (opts.json) {
          this.npm.output(
            JSON.stringify({
              user,
              org,
              userCount,
              deleted: true,
            })
          )
        } else if (opts.parseable) {
          this.npm.output(['user', 'org', 'userCount', 'deleted'].join('\t'))
          this.npm.output([user, org, userCount, true].join('\t'))
        } else if (!opts.silent && opts.loglevel !== 'silent') {
          this.npm.output(
            `Successfully removed ${user} from ${org}. You now have ${userCount} member${
              userCount === 1 ? '' : 's'
            } in this org.`
          )
        }
      })
  }

  ls (org, user, opts) {
    if (!org) {
      throw new Error('First argument `orgname` is required.')
    }

    return liborg.ls(org, opts).then(roster => {
      if (user) {
        const newRoster = {}
        if (roster[user]) {
          newRoster[user] = roster[user]
        }

        roster = newRoster
      }
      if (opts.json) {
        this.npm.output(JSON.stringify(roster, null, 2))
      } else if (opts.parseable) {
        this.npm.output(['user', 'role'].join('\t'))
        Object.keys(roster).forEach(user => {
          this.npm.output([user, roster[user]].join('\t'))
        })
      } else if (!opts.silent && opts.loglevel !== 'silent') {
        const table = new Table({ head: ['user', 'role'] })
        Object.keys(roster)
          .sort()
          .forEach(user => {
            table.push([user, roster[user]])
          })
        this.npm.output(table.toString())
      }
    })
  }
}
module.exports = Org
