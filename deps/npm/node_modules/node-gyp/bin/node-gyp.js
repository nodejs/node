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
 * Process and execute the selected command.
 */

var prog = gyp()
prog.parseArgv(process.argv)

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

/**
 * Process and execute the selected command.
 */

if (!prog.command) {
  return prog.usageAndExit()
}

prog.info('it worked if it ends with', 'ok')

if (typeof prog.commands[prog.command] != 'function') {
  cursor.fg.red().write('ERR! ')
        .fg.reset().write('Unknown command "' + prog.command + '"\n')
  cursor.fg.red().write('ERR! ')
        .fg.reset().write('not ok\n')
  process.exit(1)
}

var completed = false
prog.commands[prog.command](prog.argv, function (err) {
  completed = true
  if (err) {
    cursor.fg.red().write('ERR! ')
          .fg.reset().write(err.stack + '\n')
    cursor.fg.red().write('ERR! ')
          .fg.reset().write('not ok\n')
    return process.exit(1)
  }
  if (prog.command == 'list') {
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
  prog.info('done', 'ok')
})

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
