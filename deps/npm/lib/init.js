// initialize a package.json file

module.exports = init

var log = require('npmlog')
var npm = require('./npm.js')
var initJson = require('init-package-json')

init.usage = 'npm init [--force|-f|--yes|-y]'

function init (args, cb) {
  var dir = process.cwd()
  log.pause()
  var initFile = npm.config.get('init-module')
  if (!initJson.yes(npm.config)) {
    console.log([
      'This utility will walk you through creating a package.json file.',
      'It only covers the most common items, and tries to guess sensible defaults.',
      '',
      'See `npm help json` for definitive documentation on these fields',
      'and exactly what they do.',
      '',
      'Use `npm install <pkg> --save` afterwards to install a package and',
      'save it as a dependency in the package.json file.',
      '',
      'Press ^C at any time to quit.'
    ].join('\n'))
  }
  initJson(dir, initFile, npm.config, function (er, data) {
    log.resume()
    log.silly('package data', data)
    if (er && er.message === 'canceled') {
      log.warn('init', 'canceled')
      return cb(null, data)
    }
    log.info('init', 'written successfully')
    cb(er, data)
  })
}
