#!/usr/bin/env node

/**
 * Set the title.
 */

process.title = 'node-gyp'

/**
 * Module dependencies.
 */

var gyp = require('../')
var log = require('npmlog')

/**
 * Process and execute the selected commands.
 */

var prog = gyp()
prog.parseArgv(process.argv)

if (prog.todo.length === 0) {
  return prog.usageAndExit()
}

log.info('it worked if it ends with', 'ok')
log.info('using', 'node-gyp@%s', prog.version)
log.info('using', 'node@%s', process.versions.node)


/**
 * Change dir if -C/--directory was passed.
 */

var dir = prog.opts.directory
if (dir) {
  var fs = require('fs')
  try {
    var stat = fs.statSync(dir)
    if (stat.isDirectory()) {
      log.info('chdir', dir)
      process.chdir(dir)
    } else {
      log.warn('chdir', dir + ' is not a directory')
    }
  } catch (e) {
    if (e.code === 'ENOENT') {
      log.warn('chdir', dir + ' is not a directory')
    } else {
      log.warn('chdir', 'error during chdir() "%s"', e.message)
    }
  }
}

// start running the given commands!
var completed = false
run()

function run () {
  if (prog.todo.length === 0) {
    // done!
    completed = true
    log.info('done', 'ok')
    return
  }
  var command = prog.todo.shift()

  // is this an alias?
  if (command in prog.aliases) {
    command = prog.aliases[command]
  }

  prog.commands[command](prog.argv.slice(), function (err) {
    if (err) {
      log.error(command + ' error', err.stack)
      log.error('not ok')
      return process.exit(1)
    }
    if (command == 'list') {
      var versions = arguments[1]
      if (versions.length > 0) {
        versions.forEach(function (version) {
          console.log(version)
        })
      } else {
        console.log('No node development files installed. Use `node-gyp install` to install a version.')
      }
    } else if (arguments.length >= 2) {
      console.log.apply(console, [].slice.call(arguments, 1))
    }

    // now run the next command in the queue
    process.nextTick(run)
  })
}

process.on('exit', function (code) {
  if (!completed && !code) {
    log.error('Completion callback never invoked!')
    log.error('This is a bug in `node-gyp`, please file an Issue:')
    log.error('', '    https://github.com/TooTallNate/node-gyp/issues')
    log.error('not ok')
    process.exit(6)
  }
})

process.on('uncaughtException', function (err) {
  log.error('UNCAUGHT EXCEPTION', err.stack)
  log.error('This is a bug in `node-gyp`, please file an Issue:')
  log.error('', '    https://github.com/TooTallNate/node-gyp/issues')
  log.error('not ok')
  process.exit(7)
})
