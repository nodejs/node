#!/usr/bin/env node

/**
 * Set the title.
 */

process.title = 'node-gyp'

/**
 * Module dependencies.
 */

var gyp = require('../')
  , inspect = require('util').inspect
  , ansi = require('ansi')
  , cursor = ansi(process.stderr)

/**
 * Process and execute the selected commands.
 */

var prog = gyp()
prog.parseArgv(process.argv)

if (prog.todo.length === 0) {
  return prog.usageAndExit()
}


/**
 * Set up logging handlers.
 */

prog.on('info', function () {
  cursor.fg.green().write('info ')
        .fg.grey().write(arguments[0] + ' ')
        .reset()
  for (var i=1, l=arguments.length; i<l; i++) {
    cursor.write(arguments[i] + ' ')
  }
  cursor.write('\n')
})

prog.on('warn', function () {
  cursor.fg.yellow().bg.red().write('warn')
        .fg.reset().bg.reset().write(' ')
        .fg.grey().write(arguments[0])
        .write(' ')
        .reset()
  for (var i=1, l=arguments.length; i<l; i++) {
    cursor.write(arguments[i] + ' ')
  }
  cursor.write('\n')
})

if (prog.opts.verbose) {
  prog.on('verbose', function () {
    cursor.fg.cyan().write('verb ')
          .fg.grey().write(arguments[0] + ' ')
          .reset()
    for (var i=1, l=arguments.length; i<l; i++) {
      cursor.write(inspect(arguments[i]) + ' ')
    }
    cursor.write('\n')
  })
}

prog.on('spawn', function (command, args, proc) {
  cursor.fg.magenta().write('spawn ')
        .fg.grey().write(command + ' ')
        .reset().write(inspect(args) + '\n')
})

prog.info('it worked if it ends with', 'ok')

/**
 * Change dir if -C/--directory was passed.
 */

var dir = prog.opts.directory
if (dir) {
  var fs = require('fs')
  try {
    var stat = fs.statSync(dir)
    if (stat.isDirectory()) {
      prog.info('chdir:', dir)
      process.chdir(dir)
    } else {
      prog.warn(dir + ' is not a directory')
    }
  } catch (e) {
    prog.warn('directory:', e.message)
  }
}

// start running the given commands!
var completed = false
run()

function run () {
  if (prog.todo.length === 0) {
    // done!
    completed = true
    prog.info('done', 'ok')
    return
  }
  var command = prog.todo.shift()
  prog.commands[command](prog.argv.slice(), function (err) {
    if (err) {
      cursor.fg.red().write('ERR! ')
            .fg.reset().write(err.stack + '\n')
      cursor.fg.red().write('ERR! ')
            .fg.reset().write('not ok\n')
      return process.exit(1)
    }
    if (command == 'list') {
      var versions = arguments[1]
      if (versions.length > 0) {
        versions.forEach(function (version) {
          console.log(version)
        })
      } else {
        prog.info('No node development files installed. Use `node-gyp install` to install a version.')
      }
    } else if (arguments.length >= 2) {
      console.log.apply(console, [].slice.call(arguments, 1))
    }
    process.nextTick(run)
  })
}

process.on('exit', function (code) {
  if (!completed && !code) {
    cursor.fg.red().write('ERR! ')
          .fg.reset().write('Completion callback never invoked!\n')
    cursor.fg.red().write('ERR! ')
          .fg.reset().write('This is a bug in `node-gyp`. Please open an Issue:\n')
    cursor.fg.red().write('ERR! ')
          .fg.reset().write('  https://github.com/TooTallNate/node-gyp/issues\n')
    cursor.fg.red().write('ERR! ')
          .fg.reset().write('not ok\n')
    process.exit(6)
  }
})

process.on('uncaughtException', function (err) {
  cursor.fg.red().write('ERR! ')
        .fg.reset().write('UNCAUGHT EXCEPTION:\n')
  cursor.fg.red().write('ERR! ')
        .fg.reset().write(err.stack + '\n')
  cursor.fg.red().write('ERR! ')
        .fg.reset().write('This is a bug in `node-gyp`. Please open an Issue:\n')
  cursor.fg.red().write('ERR! ')
        .fg.reset().write('  https://github.com/TooTallNate/node-gyp/issues\n')
  cursor.fg.red().write('ERR! ')
        .fg.reset().write('not ok\n')
  process.exit(1)
})
