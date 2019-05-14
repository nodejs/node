;(function () {
  // windows: running 'npm blah' in this folder will invoke WSH, not node.
  /* globals WScript */
  if (typeof WScript !== 'undefined') {
    WScript.echo(
      'npm does not work when run\n' +
      'with the Windows Scripting Host\n\n' +
      '"cd" to a different directory,\n' +
      'or type "npm.cmd <args>",\n' +
      'or type "node npm <args>".'
    )
    WScript.quit(1)
    return
  }

  var unsupported = require('../lib/utils/unsupported.js')
  unsupported.checkForBrokenNode()

  var gfs = require('graceful-fs')
  // Patch the global fs module here at the app level
  var fs = gfs.gracefulify(require('fs'))

  var EventEmitter = require('events').EventEmitter
  var npm = module.exports = new EventEmitter()
  var npmconf = require('./config/core.js')
  var log = require('npmlog')
  var inspect = require('util').inspect

  // capture global logging
  process.on('log', function (level) {
    try {
      return log[level].apply(log, [].slice.call(arguments, 1))
    } catch (ex) {
      log.verbose('attempt to log ' + inspect(arguments) + ' crashed: ' + ex.message)
    }
  })

  var path = require('path')
  var abbrev = require('abbrev')
  var which = require('which')
  var glob = require('glob')
  var rimraf = require('rimraf')
  var parseJSON = require('./utils/parse-json.js')
  var aliases = require('./config/cmd-list').aliases
  var cmdList = require('./config/cmd-list').cmdList
  var plumbing = require('./config/cmd-list').plumbing
  var output = require('./utils/output.js')
  var startMetrics = require('./utils/metrics.js').start
  var perf = require('./utils/perf.js')

  perf.emit('time', 'npm')
  perf.on('timing', function (name, finished) {
    log.timing(name, 'Completed in', finished + 'ms')
  })

  npm.config = {
    loaded: false,
    get: function () {
      throw new Error('npm.load() required')
    },
    set: function () {
      throw new Error('npm.load() required')
    }
  }

  npm.commands = {}

  // TUNING
  npm.limit = {
    fetch: 10,
    action: 50
  }
  // ***

  npm.lockfileVersion = 1

  npm.rollbacks = []

  try {
    // startup, ok to do this synchronously
    var j = parseJSON(fs.readFileSync(
      path.join(__dirname, '../package.json')) + '')
    npm.name = j.name
    npm.version = j.version
  } catch (ex) {
    try {
      log.info('error reading version', ex)
    } catch (er) {}
    npm.version = ex
  }

  var commandCache = {}
  var aliasNames = Object.keys(aliases)

  var littleGuys = [ 'isntall', 'verison' ]
  var fullList = cmdList.concat(aliasNames).filter(function (c) {
    return plumbing.indexOf(c) === -1
  })
  var abbrevs = abbrev(fullList)

  // we have our reasons
  fullList = npm.fullList = fullList.filter(function (c) {
    return littleGuys.indexOf(c) === -1
  })

  var registryRefer

  Object.keys(abbrevs).concat(plumbing).forEach(function addCommand (c) {
    Object.defineProperty(npm.commands, c, { get: function () {
      if (!loaded) {
        throw new Error(
          'Call npm.load(config, cb) before using this command.\n' +
            'See the README.md or bin/npm-cli.js for example usage.'
        )
      }
      var a = npm.deref(c)
      if (c === 'la' || c === 'll') {
        npm.config.set('long', true)
      }

      npm.command = c
      if (commandCache[a]) return commandCache[a]

      var cmd = require(path.join(__dirname, a + '.js'))

      commandCache[a] = function () {
        var args = Array.prototype.slice.call(arguments, 0)
        if (typeof args[args.length - 1] !== 'function') {
          args.push(defaultCb)
        }
        if (args.length === 1) args.unshift([])

        // Options are prefixed by a hyphen-minus (-, \u2d).
        // Other dash-type chars look similar but are invalid.
        Array(args[0]).forEach(function (arg) {
          if (/^[\u2010-\u2015\u2212\uFE58\uFE63\uFF0D]/.test(arg)) {
            log.error('arg', 'Argument starts with non-ascii dash, this is probably invalid:', arg)
          }
        })

        if (!registryRefer) {
          registryRefer = [a].concat(args[0]).map(function (arg) {
            // exclude anything that might be a URL, path, or private module
            // Those things will always have a slash in them somewhere
            if (arg && arg.match && arg.match(/\/|\\/)) {
              return '[REDACTED]'
            } else {
              return arg
            }
          }).filter(function (arg) {
            return arg && arg.match
          }).join(' ')
          npm.referer = registryRefer
        }

        cmd.apply(npm, args)
      }

      Object.keys(cmd).forEach(function (k) {
        commandCache[a][k] = cmd[k]
      })

      return commandCache[a]
    },
    enumerable: fullList.indexOf(c) !== -1,
    configurable: true })

    // make css-case commands callable via camelCase as well
    if (c.match(/-([a-z])/)) {
      addCommand(c.replace(/-([a-z])/g, function (a, b) {
        return b.toUpperCase()
      }))
    }
  })

  function defaultCb (er, data) {
    log.disableProgress()
    if (er) console.error(er.stack || er.message)
    else output(data)
  }

  npm.deref = function (c) {
    if (!c) return ''
    if (c.match(/[A-Z]/)) {
      c = c.replace(/([A-Z])/g, function (m) {
        return '-' + m.toLowerCase()
      })
    }
    if (plumbing.indexOf(c) !== -1) return c
    var a = abbrevs[c]
    while (aliases[a]) {
      a = aliases[a]
    }
    return a
  }

  var loaded = false
  var loading = false
  var loadErr = null
  var loadListeners = []

  function loadCb (er) {
    loadListeners.forEach(function (cb) {
      process.nextTick(cb.bind(npm, er, npm))
    })
    loadListeners.length = 0
  }

  npm.load = function (cli, cb_) {
    if (!cb_ && typeof cli === 'function') {
      cb_ = cli
      cli = {}
    }
    if (!cb_) cb_ = function () {}
    if (!cli) cli = {}
    loadListeners.push(cb_)
    if (loaded || loadErr) return cb(loadErr)
    if (loading) return
    loading = true
    var onload = true

    function cb (er) {
      if (loadErr) return
      loadErr = er
      if (er) return cb_(er)
      if (npm.config.get('force')) {
        log.warn('using --force', 'I sure hope you know what you are doing.')
      }
      npm.config.loaded = true
      loaded = true
      loadCb(loadErr = er)
      onload = onload && npm.config.get('onload-script')
      if (onload) {
        try {
          require(onload)
        } catch (err) {
          log.warn('onload-script', 'failed to require onload script', onload)
          log.warn('onload-script', err)
        }
        onload = false
      }
    }

    log.pause()

    load(npm, cli, cb)
  }

  function load (npm, cli, cb) {
    which(process.argv[0], function (er, node) {
      if (!er && node.toUpperCase() !== process.execPath.toUpperCase()) {
        log.verbose('node symlink', node)
        process.execPath = node
        process.installPrefix = path.resolve(node, '..', '..')
      }

      // look up configs
      var builtin = path.resolve(__dirname, '..', 'npmrc')
      npmconf.load(cli, builtin, function (er, config) {
        if (er === config) er = null

        npm.config = config
        if (er) return cb(er)

        // if the 'project' config is not a filename, and we're
        // not in global mode, then that means that it collided
        // with either the default or effective userland config
        if (!config.get('global') &&
            config.sources.project &&
            config.sources.project.type !== 'ini') {
          log.verbose(
            'config',
            'Skipping project config: %s. (matches userconfig)',
            config.localPrefix + '/.npmrc'
          )
        }

        // Include npm-version and node-version in user-agent
        var ua = config.get('user-agent') || ''
        ua = ua.replace(/\{node-version\}/gi, process.version)
        ua = ua.replace(/\{npm-version\}/gi, npm.version)
        ua = ua.replace(/\{platform\}/gi, process.platform)
        ua = ua.replace(/\{arch\}/gi, process.arch)
        config.set('user-agent', ua)

        if (config.get('metrics-registry') == null) {
          config.set('metrics-registry', config.get('registry'))
        }

        var color = config.get('color')

        if (npm.config.get('timing') && npm.config.get('loglevel') === 'notice') {
          log.level = 'timing'
        } else {
          log.level = config.get('loglevel')
        }
        log.heading = config.get('heading') || 'npm'
        log.stream = config.get('logstream')

        switch (color) {
          case 'always':
            npm.color = true
            break
          case false:
            npm.color = false
            break
          default:
            npm.color = process.stdout.isTTY && process.env['TERM'] !== 'dumb'
            break
        }
        if (npm.color) {
          log.enableColor()
        } else {
          log.disableColor()
        }

        if (config.get('unicode')) {
          log.enableUnicode()
        } else {
          log.disableUnicode()
        }

        if (config.get('progress') && process.stderr.isTTY && process.env['TERM'] !== 'dumb') {
          log.enableProgress()
        } else {
          log.disableProgress()
        }

        glob(path.resolve(npm.cache, '_logs', '*-debug.log'), function (er, files) {
          if (er) return cb(er)

          while (files.length >= npm.config.get('logs-max')) {
            rimraf.sync(files[0])
            files.splice(0, 1)
          }
        })

        log.resume()

        var umask = npm.config.get('umask')
        npm.modes = {
          exec: parseInt('0777', 8) & (~umask),
          file: parseInt('0666', 8) & (~umask),
          umask: umask
        }

        var gp = Object.getOwnPropertyDescriptor(config, 'globalPrefix')
        Object.defineProperty(npm, 'globalPrefix', gp)

        var lp = Object.getOwnPropertyDescriptor(config, 'localPrefix')
        Object.defineProperty(npm, 'localPrefix', lp)

        config.set('scope', scopeifyScope(config.get('scope')))
        npm.projectScope = config.get('scope') ||
         scopeifyScope(getProjectScope(npm.prefix))

        startMetrics()

        return cb(null, npm)
      })
    })
  }

  Object.defineProperty(npm, 'prefix',
    {
      get: function () {
        return npm.config.get('global') ? npm.globalPrefix : npm.localPrefix
      },
      set: function (r) {
        var k = npm.config.get('global') ? 'globalPrefix' : 'localPrefix'
        npm[k] = r
        return r
      },
      enumerable: true
    })

  Object.defineProperty(npm, 'bin',
    {
      get: function () {
        if (npm.config.get('global')) return npm.globalBin
        return path.resolve(npm.root, '.bin')
      },
      enumerable: true
    })

  Object.defineProperty(npm, 'globalBin',
    {
      get: function () {
        var b = npm.globalPrefix
        if (process.platform !== 'win32') b = path.resolve(b, 'bin')
        return b
      }
    })

  Object.defineProperty(npm, 'dir',
    {
      get: function () {
        if (npm.config.get('global')) return npm.globalDir
        return path.resolve(npm.prefix, 'node_modules')
      },
      enumerable: true
    })

  Object.defineProperty(npm, 'globalDir',
    {
      get: function () {
        return (process.platform !== 'win32')
          ? path.resolve(npm.globalPrefix, 'lib', 'node_modules')
          : path.resolve(npm.globalPrefix, 'node_modules')
      },
      enumerable: true
    })

  Object.defineProperty(npm, 'root',
    { get: function () { return npm.dir } })

  Object.defineProperty(npm, 'cache',
    { get: function () { return npm.config.get('cache') },
      set: function (r) { return npm.config.set('cache', r) },
      enumerable: true
    })

  var tmpFolder
  var rand = require('crypto').randomBytes(4).toString('hex')
  Object.defineProperty(npm, 'tmp',
    {
      get: function () {
        if (!tmpFolder) tmpFolder = 'npm-' + process.pid + '-' + rand
        return path.resolve(npm.config.get('tmp'), tmpFolder)
      },
      enumerable: true
    })

  // the better to repl you with
  Object.getOwnPropertyNames(npm.commands).forEach(function (n) {
    if (npm.hasOwnProperty(n) || n === 'config') return

    Object.defineProperty(npm, n, { get: function () {
      return function () {
        var args = Array.prototype.slice.call(arguments, 0)
        var cb = defaultCb

        if (args.length === 1 && Array.isArray(args[0])) {
          args = args[0]
        }

        if (typeof args[args.length - 1] === 'function') {
          cb = args.pop()
        }
        npm.commands[n](args, cb)
      }
    },
    enumerable: false,
    configurable: true })
  })

  if (require.main === module) {
    require('../bin/npm-cli.js')
  }

  function scopeifyScope (scope) {
    return (!scope || scope[0] === '@') ? scope : ('@' + scope)
  }

  function getProjectScope (prefix) {
    try {
      var pkg = JSON.parse(fs.readFileSync(path.join(prefix, 'package.json')))
      if (typeof pkg.name !== 'string') return ''
      var sep = pkg.name.indexOf('/')
      if (sep === -1) return ''
      return pkg.name.slice(0, sep)
    } catch (ex) {
      return ''
    }
  }
})()
