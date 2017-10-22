#!/usr/bin/env node

var configUtil = require('../lib/config-util')
var foreground = require('foreground-child')
var NYC
try {
  NYC = require('../index.covered.js')
} catch (e) {
  NYC = require('../index.js')
}
var processArgs = require('../lib/process-args')

var sw = require('spawn-wrap')
var wrapper = require.resolve('./wrap.js')

// parse configuration and command-line arguments;
// we keep these values in a few different forms,
// used in the various execution contexts of nyc:
// reporting, instrumenting subprocesses, etc.
var yargs = configUtil.decorateYargs(configUtil.buildYargs())
var instrumenterArgs = processArgs.hideInstrumenteeArgs()
var argv = yargs.parse(instrumenterArgs)
var config = configUtil.loadConfig(instrumenterArgs)

if (argv._[0] === 'report') {
  // run a report.
  process.env.NYC_CWD = process.cwd()

  report(config)
} else if (argv._[0] === 'check-coverage') {
  checkCoverage(config)
} else if (argv._[0] === 'instrument') {
  // look in lib/commands/instrument.js for logic.
} else if (argv._.length) {
  // if instrument is set to false,
  // enable a noop instrumenter.
  if (!config.instrument) config.instrumenter = './lib/instrumenters/noop'
  else config.instrumenter = './lib/instrumenters/istanbul'

  var nyc = (new NYC(config))
  if (config.clean) {
    nyc.reset()
  } else {
    nyc.createTempDirectory()
  }
  if (config.all) nyc.addAllFiles()

  var env = {
    NYC_CONFIG: JSON.stringify(config),
    NYC_CWD: process.cwd(),
    NYC_ROOT_ID: nyc.rootId,
    NYC_INSTRUMENTER: config.instrumenter
  }

  if (config['babel-cache'] === false) {
    // babel's cache interferes with some configurations, so is
    // disabled by default. opt in by setting babel-cache=true.
    env.BABEL_DISABLE_CACHE = process.env.BABEL_DISABLE_CACHE = '1'
  }

  sw([wrapper], env)

  // Both running the test script invocation and the check-coverage run may
  // set process.exitCode. Keep track so that both children are run, but
  // a non-zero exit codes in either one leads to an overall non-zero exit code.
  process.exitCode = 0
  foreground(processArgs.hideInstrumenterArgs(
    // use the same argv descrption, but don't exit
    // for flags like --help.
    configUtil.buildYargs().parse(process.argv.slice(2))
  ), function (done) {
    var mainChildExitCode = process.exitCode

    if (config.checkCoverage) {
      checkCoverage(config)
      process.exitCode = process.exitCode || mainChildExitCode
      if (!config.silent) report(config)
      return done()
    } else {
      if (!config.silent) report(config)
      return done()
    }
  })
} else {
  // I don't have a clue what you're doing.
  yargs.showHelp()
}

function report (argv) {
  process.env.NYC_CWD = process.cwd()

  var nyc = new NYC(argv)
  nyc.report()
}

function checkCoverage (argv, cb) {
  process.env.NYC_CWD = process.cwd()

  ;(new NYC(argv)).checkCoverage({
    lines: argv.lines,
    functions: argv.functions,
    branches: argv.branches,
    statements: argv.statements
  }, argv['per-file'])
}
