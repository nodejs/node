/* eslint-disable standard/no-callback-literal */
var mapToRegistry = require('./utils/map-to-registry.js')
var npm = require('./npm')
var output = require('./utils/output.js')

module.exports = team

team.subcommands = ['create', 'destroy', 'add', 'rm', 'ls', 'edit']

team.usage =
  'npm team create <scope:team>\n' +
  'npm team destroy <scope:team>\n' +
  'npm team add <scope:team> <user>\n' +
  'npm team rm <scope:team> <user>\n' +
  'npm team ls <scope>|<scope:team>\n' +
  'npm team edit <scope:team>'

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

function team (args, cb) {
  // Entities are in the format <scope>:<team>
  var cmd = args.shift()
  var entity = (args.shift() || '').split(':')
  return mapToRegistry('/', npm.config, function (err, uri, auth) {
    if (err) { return cb(err) }
    try {
      return npm.registry.team(cmd, uri, {
        auth: auth,
        scope: entity[0].replace(/^@/, ''), // '@' prefix on scope is optional.
        team: entity[1],
        user: args.shift()
      }, function (err, data) {
        !err && data && output(JSON.stringify(data, undefined, 2))
        cb(err, data)
      })
    } catch (e) {
      cb(e.message + '\n\nUsage:\n' + team.usage)
    }
  })
}
