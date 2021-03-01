const liborg = require('libnpmorg')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const otplease = require('./utils/otplease.js')
const Table = require('cli-table3')

module.exports = org

org.subcommands = ['set', 'rm', 'ls']

org.usage =
  'npm org set orgname username [developer | admin | owner]\n' +
  'npm org rm orgname username\n' +
  'npm org ls orgname [<username>]'

org.completion = async (opts) => {
  var argv = opts.conf.argv.remain
  if (argv.length === 2)
    return org.subcommands

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

function UsageError () {
  throw Object.assign(new Error(org.usage), { code: 'EUSAGE' })
}

function org ([cmd, orgname, username, role], cb) {
  return otplease(npm.flatOptions, opts => {
    switch (cmd) {
      case 'add':
      case 'set':
        return orgSet(orgname, username, role, opts)
      case 'rm':
        return orgRm(orgname, username, opts)
      case 'ls':
        return orgList(orgname, username, opts)
      default:
        UsageError()
    }
  }).then(
    x => cb(null, x),
    err => cb(err.code === 'EUSAGE' ? err.message : err)
  )
}

function orgSet (org, user, role, opts) {
  role = role || 'developer'
  if (!org)
    throw new Error('First argument `orgname` is required.')

  if (!user)
    throw new Error('Second argument `username` is required.')

  if (!['owner', 'admin', 'developer'].find(x => x === role))
    throw new Error('Third argument `role` must be one of `owner`, `admin`, or `developer`, with `developer` being the default value if omitted.')

  return liborg.set(org, user, role, opts).then(memDeets => {
    if (opts.json)
      output(JSON.stringify(memDeets, null, 2))
    else if (opts.parseable) {
      output(['org', 'orgsize', 'user', 'role'].join('\t'))
      output([
        memDeets.org.name,
        memDeets.org.size,
        memDeets.user,
        memDeets.role,
      ].join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent')
      output(`Added ${memDeets.user} as ${memDeets.role} to ${memDeets.org.name}. You now have ${memDeets.org.size} member${memDeets.org.size === 1 ? '' : 's'} in this org.`)

    return memDeets
  })
}

function orgRm (org, user, opts) {
  if (!org)
    throw new Error('First argument `orgname` is required.')

  if (!user)
    throw new Error('Second argument `username` is required.')

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
        deleted: true,
      }))
    } else if (opts.parseable) {
      output(['user', 'org', 'userCount', 'deleted'].join('\t'))
      output([user, org, userCount, true].join('\t'))
    } else if (!opts.silent && opts.loglevel !== 'silent')
      output(`Successfully removed ${user} from ${org}. You now have ${userCount} member${userCount === 1 ? '' : 's'} in this org.`)
  })
}

function orgList (org, user, opts) {
  if (!org)
    throw new Error('First argument `orgname` is required.')

  return liborg.ls(org, opts).then(roster => {
    if (user) {
      const newRoster = {}
      if (roster[user])
        newRoster[user] = roster[user]

      roster = newRoster
    }
    if (opts.json)
      output(JSON.stringify(roster, null, 2))
    else if (opts.parseable) {
      output(['user', 'role'].join('\t'))
      Object.keys(roster).forEach(user => {
        output([user, roster[user]].join('\t'))
      })
    } else if (!opts.silent && opts.loglevel !== 'silent') {
      const table = new Table({ head: ['user', 'role'] })
      Object.keys(roster).sort().forEach(user => {
        table.push([user, roster[user]])
      })
      output(table.toString())
    }
  })
}
