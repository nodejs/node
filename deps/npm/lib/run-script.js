module.exports = runScript

var lifecycle = require('./utils/lifecycle.js')
var npm = require('./npm.js')
var path = require('path')
var readJson = require('read-package-json')
var log = require('npmlog')
var chain = require('slide').chain

runScript.usage = 'npm run-script <command> [-- <args>...]' +
                  '\n\nalias: npm run'

runScript.completion = function (opts, cb) {
  // see if there's already a package specified.
  var argv = opts.conf.argv.remain

  if (argv.length >= 4) return cb()

  if (argv.length === 3) {
    // either specified a script locally, in which case, done,
    // or a package, in which case, complete against its scripts
    var json = path.join(npm.localPrefix, 'package.json')
    return readJson(json, function (er, d) {
      if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
      if (er) d = {}
      var scripts = Object.keys(d.scripts || {})
      console.error('local scripts', scripts)
      if (scripts.indexOf(argv[2]) !== -1) return cb()
      // ok, try to find out which package it was, then
      var pref = npm.config.get('global') ? npm.config.get('prefix')
               : npm.localPrefix
      var pkgDir = path.resolve(pref, 'node_modules', argv[2], 'package.json')
      readJson(pkgDir, function (er, d) {
        if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
        if (er) d = {}
        var scripts = Object.keys(d.scripts || {})
        return cb(null, scripts)
      })
    })
  }

  readJson(path.join(npm.localPrefix, 'package.json'), function (er, d) {
    if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
    d = d || {}
    cb(null, Object.keys(d.scripts || {}))
  })
}

function runScript (args, cb) {
  if (!args.length) return list(cb)

  var pkgdir = npm.localPrefix
  var cmd = args.shift()

  readJson(path.resolve(pkgdir, 'package.json'), function (er, d) {
    if (er) return cb(er)
    run(d, pkgdir, cmd, args, cb)
  })
}

function list (cb) {
  var json = path.join(npm.localPrefix, 'package.json')
  var cmdList = [
    'publish',
    'install',
    'uninstall',
    'test',
    'stop',
    'start',
    'restart',
    'version'
  ].reduce(function (l, p) {
    return l.concat(['pre' + p, p, 'post' + p])
  }, [])
  return readJson(json, function (er, d) {
    if (er && er.code !== 'ENOENT' && er.code !== 'ENOTDIR') return cb(er)
    if (er) d = {}
    var allScripts = Object.keys(d.scripts || {})
    var scripts = []
    var runScripts = []
    allScripts.forEach(function (script) {
      if (cmdList.indexOf(script) !== -1) scripts.push(script)
      else runScripts.push(script)
    })

    if (log.level === 'silent') {
      return cb(null, allScripts)
    }

    if (npm.config.get('json')) {
      console.log(JSON.stringify(d.scripts || {}, null, 2))
      return cb(null, allScripts)
    }

    if (npm.config.get('parseable')) {
      allScripts.forEach(function (script) {
        console.log(script + ':' + d.scripts[script])
      })
      return cb(null, allScripts)
    }

    var s = '\n    '
    var prefix = '  '
    if (scripts.length) {
      console.log('Lifecycle scripts included in %s:', d.name)
    }
    scripts.forEach(function (script) {
      console.log(prefix + script + s + d.scripts[script])
    })
    if (!scripts.length && runScripts.length) {
      console.log('Scripts available in %s via `npm run-script`:', d.name)
    } else if (runScripts.length) {
      console.log('\navailable via `npm run-script`:')
    }
    runScripts.forEach(function (script) {
      console.log(prefix + script + s + d.scripts[script])
    })
    return cb(null, allScripts)
  })
}

function run (pkg, wd, cmd, args, cb) {
  if (!pkg.scripts) pkg.scripts = {}

  var cmds
  if (cmd === 'restart' && !pkg.scripts.restart) {
    cmds = [
      'prestop', 'stop', 'poststop',
      'restart',
      'prestart', 'start', 'poststart'
    ]
  } else {
    if (!pkg.scripts[cmd]) {
      if (cmd === 'test') {
        pkg.scripts.test = 'echo \'Error: no test specified\''
      } else if (cmd === 'env') {
        if (process.platform === 'win32') {
          log.verbose('run-script using default platform env: SET (Windows)')
          pkg.scripts[cmd] = 'SET'
        } else {
          log.verbose('run-script using default platform env: env (Unix)')
          pkg.scripts[cmd] = 'env'
        }
      } else if (npm.config.get('if-present')) {
        return cb(null)
      } else {
        return cb(new Error('missing script: ' + cmd))
      }
    }
    cmds = [cmd]
  }

  if (!cmd.match(/^(pre|post)/)) {
    cmds = ['pre' + cmd].concat(cmds).concat('post' + cmd)
  }

  log.verbose('run-script', cmds)
  chain(cmds.map(function (c) {
    // pass cli arguments after -- to script.
    if (pkg.scripts[c] && c === cmd) {
      pkg.scripts[c] = pkg.scripts[c] + joinArgs(args)
    }

    // when running scripts explicitly, assume that they're trusted.
    return [lifecycle, pkg, c, wd, true]
  }), cb)
}

// join arguments after '--' and pass them to script,
// handle special characters such as ', ", ' '.
function joinArgs (args) {
  var joinedArgs = ''
  args.forEach(function (arg) {
    joinedArgs += ' "' + arg.replace(/"/g, '\\"') + '"'
  })
  return joinedArgs
}
