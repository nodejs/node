#!/usr/bin/env node
;(function () { // wrapper in case we're in module_context mode
  // windows: running "npm blah" in this folder will invoke WSH, not node.
  /* global WScript */
  if (typeof WScript !== 'undefined') {
    WScript.echo(
      'npm does not work when run\n' +
        'with the Windows Scripting Host\n\n' +
        "'cd' to a different directory,\n" +
        "or type 'npm.cmd <args>',\n" +
        "or type 'node npm <args>'."
    )
    WScript.quit(1)
    return
  }

  process.title = 'npm'

  var unsupported = require('../lib/utils/unsupported.js')
  unsupported.checkForBrokenNode()

  var log = require('npmlog')
  log.pause() // will be unpaused when config is loaded.
  log.info('it worked if it ends with', 'ok')

  unsupported.checkForUnsupportedNode()

  var npm = require('../lib/npm.js')
  var npmconf = require('../lib/config/core.js')
  var errorHandler = require('../lib/utils/error-handler.js')
  var replaceInfo = require('../lib/utils/replace-info.js')

  var configDefs = npmconf.defs
  var shorthands = configDefs.shorthands
  var types = configDefs.types
  var nopt = require('nopt')

  // if npm is called as "npmg" or "npm_g", then
  // run in global mode.
  if (process.argv[1][process.argv[1].length - 1] === 'g') {
    process.argv.splice(1, 1, 'npm', '-g')
  }

  var args = replaceInfo(process.argv)
  log.verbose('cli', args)

  var conf = nopt(types, shorthands)
  npm.argv = conf.argv.remain
  if (npm.deref(npm.argv[0])) npm.command = npm.argv.shift()
  else conf.usage = true

  if (conf.version) {
    console.log(npm.version)
    return errorHandler.exit(0)
  }

  if (conf.versions) {
    npm.command = 'version'
    conf.usage = false
    npm.argv = []
  }

  log.info('using', 'npm@%s', npm.version)
  log.info('using', 'node@%s', process.version)

  process.on('uncaughtException', errorHandler)
  process.on('unhandledRejection', errorHandler)

  if (conf.usage && npm.command !== 'help') {
    npm.argv.unshift(npm.command)
    npm.command = 'help'
  }

  var isGlobalNpmUpdate = conf.global && ['install', 'update'].includes(npm.command) && npm.argv.includes('npm')

  // now actually fire up npm and run the command.
  // this is how to use npm programmatically:
  conf._exit = true
  npm.load(conf, function (er) {
    if (er) return errorHandler(er)
    if (
      !isGlobalNpmUpdate &&
      npm.config.get('update-notifier') &&
      !unsupported.checkVersion(process.version).unsupported
    ) {
      const pkg = require('../package.json')
      let notifier = require('update-notifier')({pkg})
      const isCI = require('ci-info').isCI
      if (
        notifier.update &&
        notifier.update.latest !== pkg.version &&
        !isCI
      ) {
        const color = require('ansicolors')
        const useColor = npm.config.get('color')
        const useUnicode = npm.config.get('unicode')
        const old = notifier.update.current
        const latest = notifier.update.latest
        let type = notifier.update.type
        if (useColor) {
          switch (type) {
            case 'major':
              type = color.red(type)
              break
            case 'minor':
              type = color.yellow(type)
              break
            case 'patch':
              type = color.green(type)
              break
          }
        }
        const changelog = `https://github.com/npm/cli/releases/tag/v${latest}`
        notifier.notify({
          message: `New ${type} version of ${pkg.name} available! ${
            useColor ? color.red(old) : old
          } ${useUnicode ? '→' : '->'} ${
            useColor ? color.green(latest) : latest
          }\n` +
          `${
            useColor ? color.yellow('Changelog:') : 'Changelog:'
          } ${
            useColor ? color.cyan(changelog) : changelog
          }\n` +
          `Run ${
            useColor
              ? color.green(`npm install -g ${pkg.name}`)
              : `npm i -g ${pkg.name}`
          } to update!`
        })
      }
    }
    npm.commands[npm.command](npm.argv, function (err) {
      // https://genius.com/Lin-manuel-miranda-your-obedient-servant-lyrics
      if (
        !err &&
        npm.config.get('ham-it-up') &&
        !npm.config.get('json') &&
        !npm.config.get('parseable') &&
        npm.command !== 'completion'
      ) {
        console.error(
          `\n ${
            npm.config.get('unicode') ? '🎵 ' : ''
          } I Have the Honour to Be Your Obedient Servant,${
            npm.config.get('unicode') ? '🎵 ' : ''
          } ~ npm ${
            npm.config.get('unicode') ? '📜🖋 ' : ''
          }\n`
        )
      }
      errorHandler.apply(this, arguments)
    })
  })
})()
