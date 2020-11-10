// initialize a package.json file

module.exports = init

var path = require('path')
var log = require('npmlog')
var npa = require('npm-package-arg')
var npm = require('./npm.js')
var npx = require('libnpx')
var initJson = require('init-package-json')
var isRegistry = require('./utils/is-registry.js')
var output = require('./utils/output.js')
var noProgressTillDone = require('./utils/no-progress-while-running').tillDone
var usage = require('./utils/usage')

init.usage = usage(
  'init',
  '\nnpm init [--force|-f|--yes|-y|--scope]' +
  '\nnpm init <@scope> (same as `npx <@scope>/create`)' +
  '\nnpm init [<@scope>/]<name> (same as `npx [<@scope>/]create-<name>`)'
)

function init (args, cb) {
  if (args.length) {
    var NPM_PATH = path.resolve(__dirname, '../bin/npm-cli.js')
    var initerName = args[0]
    var packageName = initerName
    if (/^@[^/]+$/.test(initerName)) {
      packageName = initerName + '/create'
    } else {
      var req = npa(initerName)
      if (req.type === 'git' && req.hosted) {
        var { user, project } = req.hosted
        packageName = initerName
          .replace(user + '/' + project, user + '/create-' + project)
      } else if (isRegistry(req)) {
        packageName = req.name.replace(/^(@[^/]+\/)?/, '$1create-')
        if (req.rawSpec) {
          packageName += '@' + req.rawSpec
        }
      } else {
        var err = new Error(
          'Unrecognized initializer: ' + initerName +
          '\nFor more package binary executing power check out `npx`:' +
          '\nhttps://www.npmjs.com/package/npx'
        )
        err.code = 'EUNSUPPORTED'
        throw err
      }
    }
    var npxArgs = [process.argv0, '[fake arg]', '--always-spawn', packageName, ...process.argv.slice(4)]
    var parsed = npx.parseArgs(npxArgs, NPM_PATH)

    return npx(parsed)
      .then(() => cb())
      .catch(cb)
  }
  var dir = process.cwd()
  log.pause()
  var initFile = npm.config.get('init-module')
  if (!initJson.yes(npm.config)) {
    output([
      'This utility will walk you through creating a package.json file.',
      'It only covers the most common items, and tries to guess sensible defaults.',
      '',
      'See `npm help init` for definitive documentation on these fields',
      'and exactly what they do.',
      '',
      'Use `npm install <pkg>` afterwards to install a package and',
      'save it as a dependency in the package.json file.',
      '',
      'Press ^C at any time to quit.'
    ].join('\n'))
  }
  initJson(dir, initFile, npm.config, noProgressTillDone(function (er, data) {
    log.resume()
    log.silly('package data', data)
    if (er && er.message === 'canceled') {
      log.warn('init', 'canceled')
      return cb(null, data)
    }
    log.info('init', 'written successfully')
    cb(er, data)
  }))
}
