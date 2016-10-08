#!/usr/bin/env node
var node = process.execPath
var fs = require('fs')
var spawn = require('child_process').spawn
var fg = require('foreground-child')
var opener = require('opener')
var colorSupport = require('color-support')
var nycBin = require.resolve('nyc/bin/nyc.js')
var glob = require('glob')
var isexe = require('isexe')
var osHomedir = require('os-homedir')
var yaml = require('js-yaml')

var coverageServiceTest = process.env.COVERAGE_SERVICE_TEST === 'true'

// console.error(process.argv.join(' '))
// console.error('CST=%j', process.env.COVERAGE_SERVICE_TEST)
// console.error('CRT=%j', process.env.COVERALLS_REPO_TOKEN)
if (coverageServiceTest) {
  console.log('COVERAGE_SERVICE_TEST')
}

// Add new coverage services here.
// it'll check for the environ named and pipe appropriately.
//
// Currently only Coveralls is supported, but the infrastructure
// is left in place in case some noble soul fixes the codecov
// module in the future.  See https://github.com/tapjs/node-tap/issues/270
var coverageServices = [
  {
    env: 'COVERALLS_REPO_TOKEN',
    bin: require.resolve('coveralls/bin/coveralls.js'),
    name: 'Coveralls'
  }
]

main()

function main () {
  var args = process.argv.slice(2)

  if (!args.length && process.stdin.isTTY) {
    console.error(usage())
    process.exit(1)
  }

  // set default args
  var defaults = constructDefaultArgs()

  // parse dotfile
  var rcFile = process.env.TAP_RCFILE || (osHomedir() + '/.taprc')
  var rcOptions = parseRcFile(rcFile)

  // supplement defaults with parsed rc options
  if (rcOptions) {
    Object.keys(rcOptions).forEach(function (k) {
      defaults[k] = rcOptions[k]
    })
  }

  defaults.rcFile = rcFile

  // parse args
  var options = parseArgs(args, defaults)

  if (!options) {
    return
  }

  process.stdout.on('error', function (er) {
    if (er.code === 'EPIPE') {
      process.exit()
    } else {
      throw er
    }
  })

  options.files = globFiles(options.files)

  if ((options.coverageReport || options.checkCoverage) &&
      options.files.length === 0) {
    runCoverageReport(options)
    return
  }

  if (options.files.length === 0) {
    console.error('Reading TAP data from stdin (use "-" argument to suppress)')
    options.files.push('-')
  }

  if (options.files.length === 1 && options.files[0] === '-') {
    if (options.coverage) {
      console.error('Coverage disabled because stdin cannot be instrumented')
    }
    stdinOnly(options)
    return
  }

  // By definition, the next block cannot be covered, because
  // they are only relevant when coverage is turned off.

  /* istanbul ignore if */
  if (options.coverage && !global.__coverage__) {
    respawnWithCoverage(options)
    return
  }

  setupTapEnv(options)

  runTests(options)
}

function constructDefaultArgs () {
  var defaultTimeout = 30
  if (global.__coverage__) {
    defaultTimeout = 240
  }

  var defaultArgs = {
    nodeArgs: [],
    nycArgs: [],
    testArgs: [],
    timeout: +process.env.TAP_TIMEOUT || defaultTimeout,
    color: !!colorSupport.level,
    reporter: null,
    files: [],
    bail: false,
    saveFile: null,
    pipeToService: false,
    coverageReport: null,
    browser: true,
    coverage: undefined,
    checkCoverage: false,
    branches: 0,
    functions: 0,
    lines: 0,
    statements: 0
  }

  if (process.env.TAP_COLORS !== undefined) {
    defaultArgs.color = !!(+process.env.TAP_COLORS)
  }

  return defaultArgs
}

function parseArgs (args, defaults) {
  var options = defaults || {}

  var singleFlags = {
    b: 'bail',
    B: 'no-bail',
    c: 'color',
    C: 'no-color',
    h: 'help',
    '?': 'help',
    v: 'version'
  }
  var singleOpts = {
    R: 'reporter',
    t: 'timeout',
    s: 'save'
  }

  // If we're running under Travis-CI with a Coveralls.io token,
  // then it's a safe bet that we ought to output coverage.
  for (var i = 0; i < coverageServices.length && !options.pipeToService; i++) {
    if (process.env[coverageServices[i].env]) {
      options.pipeToService = true
    }
  }

  var defaultCoverage = options.pipeToService
  var dumpConfig = false

  for (i = 0; i < args.length; i++) {
    var arg = args[i]
    if (arg.charAt(0) !== '-' || arg === '-') {
      options.files.push(arg)
      continue
    }

    // short-flags
    if (arg.charAt(1) !== '-' && arg !== '-gc') {
      var expand = []
      for (var f = 1; f < arg.length; f++) {
        var fc = arg.charAt(f)
        var sf = singleFlags[fc]
        var so = singleOpts[fc]
        if (sf) {
          expand.push('--' + sf)
        } else if (so) {
          var soval = arg.slice(f + 1)
          if (soval.charAt(0) !== '=') {
            soval = '=' + soval
          }
          expand.push('--' + so + soval)
          f = arg.length
        } else if (arg !== '-' + fc) {
          expand.push('-' + fc)
        }
      }
      if (expand.length) {
        args.splice.apply(args, [i, 1].concat(expand))
        i--
        continue
      }
    }

    var key = arg
    var val = null
    if (key.match(/^--/) && arg.indexOf('=') !== -1) {
      var kv = arg.split('=')
      key = kv.shift()
      val = kv.join('=')
    }

    switch (key) {
      case '--help':
        console.log(usage())
        return null

      case '--dump-config':
        dumpConfig = true
        continue

      case '--nyc-help':
        nycHelp()
        return null

      case '--nyc-version':
        nycVersion()
        return null

      case '--version':
        console.log(require('../package.json').version)
        return null

      case '--__coverage__':
        // NYC will not wrap a module in node_modules.
        // So, we need to tell the child proc when it's been added.
        global.__coverage__ = global.__coverage__ || {}
        continue

      case '--coverage-report':
        options.coverageReport = val || args[++i]
        if (options.coverageReport === 'html') {
          options.coverageReport = 'lcov'
        }
        defaultCoverage = true
        continue

      case '--no-browser':
        options.browser = false
        continue

      case '--no-coverage-report':
        options.coverageReport = false
        continue

      case '--no-cov': case '--no-coverage':
        options.coverage = false
        continue

      case '--cov': case '--coverage':
        options.coverage = true
        continue

      case '--save':
        val = val || args[++i]
        options.saveFile = val
        continue

      case '--reporter':
        val = val || args[++i]
        options.reporter = val
        continue

      case '--gc': case '-gc': case '--expose-gc':
        options.nodeArgs.push('--expose-gc')
        continue

      case '--strict':
        options.nodeArgs.push('--use_strict')
        continue

      case '--debug':
        options.nodeArgs.push('--debug')
        continue

      case '--debug-brk':
        options.nodeArgs.push('--debug-brk')
        continue

      case '--harmony':
        options.nodeArgs.push('--harmony')
        continue

      case '--node-arg':
        val = val || args[++i]
        if (val !== undefined) {
          options.nodeArgs.push(val)
        }
        continue

      case '--check-coverage':
        defaultCoverage = true
        options.checkCoverage = true
        continue

      case '--test-arg':
        val = val || args[++i]
        if (val !== undefined) {
          options.testArgs.push(val)
        }
        continue

      case '--nyc-arg':
        val = val || args[++i]
        if (val !== undefined) {
          options.nycArgs.push(val)
        }
        continue

      case '--100':
        defaultCoverage = true
        options.checkCoverage = true
        options.branches = 100
        options.functions = 100
        options.lines = 100
        options.statements = 100
        continue

      case '--branches':
      case '--functions':
      case '--lines':
      case '--statements':
        defaultCoverage = true
        val = val || args[++i]
        options.checkCoverage = true
        options[key.slice(2)] = val
        continue

      case '--color':
        options.color = true
        continue

      case '--no-color':
        options.color = false
        continue

      case '--timeout':
        val = val || args[++i]
        options.timeout = +val
        continue

      case '--bail':
        options.bail = true
        continue

      case '--no-bail':
        options.bail = false
        continue

      case '--':
        options.files = options.files.concat(args.slice(i + 1))
        i = args.length
        continue

      default:
        throw new Error('Unknown argument: ' + arg)
    }
  }

  if (options.coverage === undefined) {
    options.coverage = defaultCoverage
  }

  if (process.env.TAP === '1') {
    options.reporter = 'tap'
  }

  // default to tap for non-tty envs
  if (!options.reporter) {
    options.reporter = options.color ? 'classic' : 'tap'
  }

  if (dumpConfig) {
    console.log(JSON.stringify(options, null, 2))
    return false
  }

  return options
}

/* istanbul ignore next */
function respawnWithCoverage (options) {
  // console.error('respawn with coverage')
  // Re-spawn with coverage
  var args = [nycBin].concat(
    '--silent',
    options.nycArgs,
    '--',
    process.execArgv,
    process.argv.slice(1),
    '--__coverage__'
  )
  var child = fg(node, args)
  child.removeAllListeners('close')
  child.on('close', function (code, signal) {
    runCoverageReport(options, code, signal)
  })
}

function pipeToCoverageServices (options, child) {
  // console.error('pipe to services')
  var piped = false
  for (var i = 0; i < coverageServices.length; i++) {
    // console.error('pipe to service?', coverageServices[i].env)
    if (process.env[coverageServices[i].env]) {
      pipeToCoverageService(coverageServices[i], options, child)
      piped = true
    }
  }

  /* istanbul ignore if */
  if (!piped) {
    throw new Error('unknown service, internal error')
  }
}

function pipeToCoverageService (service, options, child) {
  // console.error('pipe to service yes', service.env)
  var bin = service.bin

  if (coverageServiceTest) {
    // console.error('use fakey test bin')
    // test scaffolding.
    // don't actually send stuff to the service
    bin = require.resolve('../test/fixtures/cat.js')
    console.log('%s:%s', service.name, process.env[service.env])
  }

  var ca = spawn(node, [bin], {
    stdio: [ 'pipe', 1, 2 ]
  })

  child.stdout.pipe(ca.stdin)

  ca.on('close', function (code, signal) {
    if (signal) {
      process.kill(process.pid, signal)
    } else if (code) {
      console.log('Error piping coverage to ' + service.name)
    } else {
      console.log('Successfully piped to ' + service.name)
    }
  })
}

function runCoverageReport (options, code, signal) {
  if (options.checkCoverage) {
    runCoverageCheck(options, code, signal)
  } else {
    runCoverageReportOnly(options, code, signal)
  }
}

function runCoverageReportOnly (options, code, signal) {
  if (options.coverageReport === false) {
    return close(code, signal)
  }

  if (!options.coverageReport) {
    if (options.pipeToService || coverageServiceTest) {
      options.coverageReport = 'text-lcov'
    } else {
      options.coverageReport = 'text'
    }
  }

  var args = [nycBin, 'report', '--reporter', options.coverageReport]
  // console.error('run coverage report', args)

  var child
  // automatically hook into coveralls
  if (options.coverageReport === 'text-lcov' && options.pipeToService) {
    // console.error('pipeToService')
    child = spawn(node, args, { stdio: [ 0, 'pipe', 2 ] })
    pipeToCoverageServices(options, child)
  } else {
    // otherwise just run the reporter
    child = fg(node, args)
    if (options.coverageReport === 'lcov' && options.browser) {
      child.on('exit', function () {
        opener('coverage/lcov-report/index.html')
      })
    }
  }

  if (code || signal) {
    child.removeAllListeners('close')
    child.on('close', close)
  }

  function close (c, s) {
    if (signal || s) {
      setTimeout(function () {}, 200)
      return process.kill(process.pid, signal || s)
    }
    if (code || c) {
      return process.exit(code || c)
    }
  }
}

function coverageCheckArgs (options) {
  var args = []
  if (options.branches) {
    args.push('--branches', options.branches)
  }
  if (options.functions) {
    args.push('--functions', options.functions)
  }
  if (options.lines) {
    args.push('--lines', options.lines)
  }
  if (options.statements) {
    args.push('--statements', options.statements)
  }

  return args
}

function runCoverageCheck (options, code, signal) {
  var args = [nycBin, 'check-coverage'].concat(coverageCheckArgs(options))

  var child = fg(node, args)
  child.removeAllListeners('close')
  child.on('close', function (c, s) {
    runCoverageReportOnly(options, code || c, signal || s)
  })
}

function usage () {
  return fs.readFileSync(__dirname + '/usage.txt', 'utf8')
    .split('@@REPORTERS@@')
    .join(getReporters())
}

function nycHelp () {
  fg(node, [nycBin, '--help'])
}

function nycVersion () {
  console.log(require('nyc/package.json').version)
}

function getReporters () {
  var types = require('tap-mocha-reporter').types
  types = types.reduce(function (str, t) {
    var ll = str.split('\n').pop().length + t.length
    if (ll < 40) {
      return str + ' ' + t
    } else {
      return str + '\n' + t
    }
  }, '').trim()
  var ind = '                              '
  return ind + types.split('\n').join('\n' + ind)
}

function setupTapEnv (options) {
  process.env.TAP_TIMEOUT = options.timeout
  if (options.color) {
    process.env.TAP_COLORS = 1
  } else {
    process.env.TAP_COLORS = 0
  }

  if (options.bail) {
    process.env.TAP_BAIL = '1'
  }
}

function globFiles (files) {
  return files.reduce(function (acc, f) {
    if (f === '-') {
      acc.push(f)
      return acc
    }

    // glob claims patterns MUST not include any '\'s
    if (!/\\/.test(f)) {
      f = glob.sync(f) || f
    }
    return acc.concat(f)
  }, [])
}

function makeReporter (options) {
  var TMR = require('tap-mocha-reporter')
  return new TMR(options.reporter)
}

function stdinOnly (options) {
  // if we didn't specify any files, then just passthrough
  // to the reporter, so we don't get '/dev/stdin' in the suite list.
  // We have to pause() before piping to switch streams2 into old-mode
  process.stdin.pause()
  var reporter = makeReporter(options)
  process.stdin.pipe(reporter)
  process.stdin.resume()
}

function readSaveFile (options) {
  if (options.saveFile) {
    try {
      return fs.readFileSync(options.saveFile, 'utf8').trim().split('\n')
    } catch (er) {}
  }
  return options.files
}

function saveFails (options, tap) {
  if (!options.saveFile) {
    return
  }
  var fails = []
  var successes = []
  tap.on('result', function (res) {
    // we will continue to re-run todo tests, even though they're
    // not technically "failures".
    if (!res.ok && !res.extra.skip) {
      fails.push(res.extra.file)
    } else {
      successes.push(res.extra.file)
    }
  })

  tap.on('bailout', function (reason) {
    // add any pending test files to the fails list.
    fails.push.apply(fails, options.files.filter(function (file) {
      return successes.indexOf(file) === -1
    }))
    save()
  })

  tap.on('end', save)

  function save () {
    if (!fails.length) {
      try {
        fs.unlinkSync(options.saveFile)
      } catch (er) {}
    } else {
      try {
        fs.writeFileSync(options.saveFile, fails.join('\n') + '\n')
      } catch (er) {}
    }
  }
}

function runAllFiles (options, saved, tap) {
  var doStdin = false
  var opt = {
    env: Object.keys(process.env).reduce(function (env, k) {
      if (!env[k]) {
        env[k] = process.env[k]
      }
      return env
    }, {
      TAP: 1
    })
  }

  options.files = options.files.filter(function (file) {
    return saved.indexOf(file) !== -1
  })

  for (var i = 0; i < options.files.length; i++) {
    var file = options.files[i]

    // Pick up stdin after all the other files are handled.
    if (file === '-') {
      doStdin = true
      continue
    }

    var st = fs.statSync(options.files[i])
    var extra = {}
    if (options.timeout) {
      extra.timeout = options.timeout * 1000
    }

    extra.file = file

    if (file.match(/\.js$/)) {
      var args = options.nodeArgs.concat(file).concat(options.testArgs)
      tap.spawn(node, args, opt, file, extra)
    } else if (st.isDirectory()) {
      var dir = fs.readdirSync(file).map(function (f) {
        return file + '/' + f
      })
      options.files.push.apply(options.files, dir)
    } else if (isexe.sync(options.files[i])) {
      tap.spawn(options.files[i], options.testArgs, opt, file, extra)
    }
  }

  if (doStdin) {
    tap.stdin()
  }
}

function runTests (options) {
  var saved = readSaveFile(options)

  // At this point, we know we need to use the tap root,
  // because there are 1 or more files to spawn.
  var tap = require('../lib/root.js')

  // if not -Rtap, then output what the user wants.
  if (options.reporter !== 'tap') {
    tap.unpipe(process.stdout)
    tap.pipe(makeReporter(options))
  }

  saveFails(options, tap)

  runAllFiles(options, saved, tap)

  tap.end()
}

function parseRcFile (path) {
  try {
    var contents = fs.readFileSync(path, 'utf8')
    return yaml.safeLoad(contents) || {}
  } catch(er) {
    // if no dotfile exists, or invalid yaml, fail gracefully
    return {}
  }
}
