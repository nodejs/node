var NYC
try {
  NYC = require('../../index.covered.js')
} catch (e) {
  NYC = require('../../index.js')
}

exports.command = 'instrument <input> [output]'

exports.describe = 'instruments a file or a directory tree and writes the instrumented code to the desired output location'

exports.builder = function (yargs) {
  return yargs
    .usage('$0 instrument <input> [output]')
    .option('require', {
      alias: 'i',
      default: [],
      describe: 'a list of additional modules that nyc should attempt to require in its subprocess, e.g., babel-register, babel-polyfill.'
    })
    .option('extension', {
      alias: 'e',
      default: [],
      describe: 'a list of extensions that nyc should handle in addition to .js'
    })
    .option('source-map', {
      default: true,
      type: 'boolean',
      description: 'should nyc detect and handle source maps?'
    })
    .option('instrument', {
      default: true,
      type: 'boolean',
      description: 'should nyc handle instrumentation?'
    })
    .example('$0 instrument ./lib ./output', 'instrument all .js files in ./lib with coverage and output in ./output')
}

exports.handler = function (argv) {
  // if instrument is set to false,
  // enable a noop instrumenter.
  if (!argv.instrument) argv.instrumenter = './lib/instrumenters/noop'
  else argv.instrumenter = './lib/instrumenters/istanbul'

  var nyc = new NYC({
    instrumenter: argv.instrumenter,
    sourceMap: argv.sourceMap,
    extension: argv.extension,
    require: argv.require
  })

  nyc.instrumentAllFiles(argv.input, argv.output, function (err) {
    if (err) console.error(err.message)
    process.exit(1)
  })
}
