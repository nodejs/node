/* eslint-disable standard/no-callback-literal */
module.exports = config

var log = require('npmlog')
var npm = require('./npm.js')
var npmconf = require('./config/core.js')
var fs = require('graceful-fs')
var writeFileAtomic = require('write-file-atomic')
var types = npmconf.defs.types
var ini = require('ini')
var editor = require('editor')
var os = require('os')
var path = require('path')
var mkdirp = require('gentle-fs').mkdir
var umask = require('./utils/umask')
var usage = require('./utils/usage')
var output = require('./utils/output')
var noProgressTillDone = require('./utils/no-progress-while-running').tillDone

config.usage = usage(
  'config',
  'npm config set <key> <value>' +
  '\nnpm config get [<key>]' +
  '\nnpm config delete <key>' +
  '\nnpm config list [--json]' +
  '\nnpm config edit' +
  '\nnpm set <key> <value>' +
  '\nnpm get [<key>]'
)
config.completion = function (opts, cb) {
  var argv = opts.conf.argv.remain
  if (argv[1] !== 'config') argv.unshift('config')
  if (argv.length === 2) {
    var cmds = ['get', 'set', 'delete', 'ls', 'rm', 'edit']
    if (opts.partialWord !== 'l') cmds.push('list')
    return cb(null, cmds)
  }

  var action = argv[2]
  switch (action) {
    case 'set':
      // todo: complete with valid values, if possible.
      if (argv.length > 3) return cb(null, [])
      // fallthrough
      /* eslint no-fallthrough:0 */
    case 'get':
    case 'delete':
    case 'rm':
      return cb(null, Object.keys(types))
    case 'edit':
    case 'list':
    case 'ls':
      return cb(null, [])
    default:
      return cb(null, [])
  }
}

// npm config set key value
// npm config get key
// npm config list
function config (args, cb) {
  var action = args.shift()
  switch (action) {
    case 'set':
      return set(args[0], args[1], cb)
    case 'get':
      return get(args[0], cb)
    case 'delete':
    case 'rm':
    case 'del':
      return del(args[0], cb)
    case 'list':
    case 'ls':
      return npm.config.get('json') ? listJson(cb) : list(cb)
    case 'edit':
      return edit(cb)
    default:
      return unknown(action, cb)
  }
}

function edit (cb) {
  var e = npm.config.get('editor')
  var which = npm.config.get('global') ? 'global' : 'user'
  var f = npm.config.get(which + 'config')
  if (!e) return cb(new Error('No EDITOR config or environ set.'))
  npm.config.save(which, function (er) {
    if (er) return cb(er)
    fs.readFile(f, 'utf8', function (er, data) {
      if (er) data = ''
      data = [
        ';;;;',
        '; npm ' + (npm.config.get('global')
          ? 'globalconfig' : 'userconfig') + ' file',
        '; this is a simple ini-formatted file',
        '; lines that start with semi-colons are comments.',
        '; read `npm help config` for help on the various options',
        ';;;;',
        '',
        data
      ].concat([
        ';;;;',
        '; all options with default values',
        ';;;;'
      ]).concat(Object.keys(npmconf.defaults).reduce(function (arr, key) {
        var obj = {}
        obj[key] = npmconf.defaults[key]
        if (key === 'logstream') return arr
        return arr.concat(
          ini.stringify(obj)
            .replace(/\n$/m, '')
            .replace(/^/g, '; ')
            .replace(/\n/g, '\n; ')
            .split('\n'))
      }, []))
        .concat([''])
        .join(os.EOL)
      mkdirp(path.dirname(f), function (er) {
        if (er) return cb(er)
        writeFileAtomic(
          f,
          data,
          function (er) {
            if (er) return cb(er)
            editor(f, { editor: e }, noProgressTillDone(cb))
          }
        )
      })
    })
  })
}

function del (key, cb) {
  if (!key) return cb(new Error('no key provided'))
  var where = npm.config.get('global') ? 'global' : 'user'
  npm.config.del(key, where)
  npm.config.save(where, cb)
}

function set (key, val, cb) {
  if (key === undefined) {
    return unknown('', cb)
  }
  if (val === undefined) {
    if (key.indexOf('=') !== -1) {
      var k = key.split('=')
      key = k.shift()
      val = k.join('=')
    } else {
      val = ''
    }
  }
  key = key.trim()
  val = val.trim()
  log.info('config', 'set %j %j', key, val)
  var where = npm.config.get('global') ? 'global' : 'user'
  if (key.match(/umask/)) val = umask.fromString(val)
  npm.config.set(key, val, where)
  npm.config.save(where, cb)
}

function get (key, cb) {
  if (!key) return list(cb)
  if (!publicVar(key)) {
    return cb(new Error('---sekretz---'))
  }
  var val = npm.config.get(key)
  if (key.match(/umask/)) val = umask.toString(val)
  output(val)
  cb()
}

function sort (a, b) {
  return a > b ? 1 : -1
}

function publicVar (k) {
  return !(k.charAt(0) === '_' || k.indexOf(':_') !== -1)
}

function getKeys (data) {
  return Object.keys(data).filter(publicVar).sort(sort)
}

function listJson (cb) {
  const publicConf = npm.config.keys.reduce((publicConf, k) => {
    var value = npm.config.get(k)

    if (publicVar(k) &&
        // argv is not really config, it's command config
        k !== 'argv' &&
        // logstream is a Stream, and would otherwise produce circular refs
        k !== 'logstream') publicConf[k] = value

    return publicConf
  }, {})

  output(JSON.stringify(publicConf, null, 2))
  return cb()
}

function listFromSource (title, conf, long) {
  var confKeys = getKeys(conf)
  var msg = ''

  if (confKeys.length) {
    msg += '; ' + title + '\n'
    confKeys.forEach(function (k) {
      var val = JSON.stringify(conf[k])
      if (conf[k] !== npm.config.get(k)) {
        if (!long) return
        msg += '; ' + k + ' = ' + val + ' (overridden)\n'
      } else msg += k + ' = ' + val + '\n'
    })
    msg += '\n'
  }

  return msg
}

function list (cb) {
  var msg = ''
  var long = npm.config.get('long')

  var cli = npm.config.sources.cli.data
  var cliKeys = getKeys(cli)
  if (cliKeys.length) {
    msg += '; cli configs\n'
    cliKeys.forEach(function (k) {
      if (cli[k] && typeof cli[k] === 'object') return
      if (k === 'argv') return
      msg += k + ' = ' + JSON.stringify(cli[k]) + '\n'
    })
    msg += '\n'
  }

  // env configs
  msg += listFromSource('environment configs', npm.config.sources.env.data, long)

  // project config file
  var project = npm.config.sources.project
  msg += listFromSource('project config ' + project.path, project.data, long)

  // user config file
  msg += listFromSource('userconfig ' + npm.config.get('userconfig'), npm.config.sources.user.data, long)

  // global config file
  msg += listFromSource('globalconfig ' + npm.config.get('globalconfig'), npm.config.sources.global.data, long)

  // builtin config file
  var builtin = npm.config.sources.builtin || {}
  if (builtin && builtin.data) {
    msg += listFromSource('builtin config ' + builtin.path, builtin.data, long)
  }

  // only show defaults if --long
  if (!long) {
    msg += '; node bin location = ' + process.execPath + '\n' +
           '; cwd = ' + process.cwd() + '\n' +
           '; HOME = ' + process.env.HOME + '\n' +
           '; "npm config ls -l" to show all defaults.\n'

    output(msg)
    return cb()
  }

  var defaults = npmconf.defaults
  var defKeys = getKeys(defaults)
  msg += '; default values\n'
  defKeys.forEach(function (k) {
    if (defaults[k] && typeof defaults[k] === 'object') return
    var val = JSON.stringify(defaults[k])
    if (defaults[k] !== npm.config.get(k)) {
      msg += '; ' + k + ' = ' + val + ' (overridden)\n'
    } else msg += k + ' = ' + val + '\n'
  })
  msg += '\n'

  output(msg)
  return cb()
}

function unknown (action, cb) {
  cb('Usage:\n' + config.usage)
}
