
module.exports = exports = build

/**
 * Module dependencies.
 */

var fs = require('fs')
  , path = require('path')
  , glob = require('glob')
  , which = require('which')
  , asyncEmit = require('./util/asyncEmit')
  , createHook = require('./util/hook')
  , win = process.platform == 'win32'

exports.usage = 'Invokes `' + (win ? 'msbuild' : 'make') + '` and builds the module'

function build (gyp, argv, callback) {

  gyp.verbose('build args', argv)
  var command = win ? 'msbuild' : 'make'
    , buildDir = path.resolve('build')
    , configPath = path.resolve(buildDir, 'config.gypi')
    , config
    , emitter

  createHook('gyp-build.js', function (err, _e) {
    if (err) return callback(err)
    emitter = _e
    loadConfigGypi()
  })

  /**
   * Load the "config.gypi" file that was generated during "configure".
   */

  function loadConfigGypi () {
    fs.readFile(configPath, 'utf8', function (err, data) {
      if (err) {
        if (err.code == 'ENOENT') {
          callback(new Error('You must run `node-gyp configure` first!'))
        } else {
          callback(err)
        }
        return
      }
      config = JSON.parse(data.replace(/\#.+\n/, ''))
      if (win) {
        findSolutionFile()
      } else {
        doWhich()
      }
    })
  }

  /**
   * On Windows, find first build/*.sln file.
   */

  function findSolutionFile () {
    glob('build/*.sln', function (err, files) {
      if (err) return callback(err)
      if (files.length === 0) {
        return callback(new Error('Could not find *.sln file. Did you run "configure"?'))
      }
      guessedSolution = files[0]
      gyp.verbose('found first Solution file', guessedSolution)
      doWhich()
    })
  }

  function doWhich () {
    // First make sure we have the build command in the PATH
    which(command, function (err, execPath) {
      if (err) {
        if (win && /not found/.test(err.message)) {
          // On windows and no 'msbuild' found. Let's guess where it is
          guessMsbuild()
        } else {
          // Some other error or 'make' not found on Unix, report that to the user
          callback(err)
        }
        return
      }
      gyp.verbose('`which` succeeded for `' + command + '`', execPath)
      build()
    })
  }

  /**
   * Guess the location of the "msbuild.exe" file on Windows.
   */

  function guessMsbuild () {
    gyp.verbose('could not find "msbuild.exe". guessing location')
    // This is basically just hard-coded. If this causes problems
    // then we'll think of something more clever.
    var windir = process.env.WINDIR || process.env.SYSTEMROOT || 'C:\\WINDOWS'
      , frameworkDir = path.resolve(windir, 'Microsoft.NET', 'Framework')
      , versionDir = path.resolve(frameworkDir, 'v4.0.30319') // This is probably the most brittle part...
      , msbuild = path.resolve(versionDir, 'msbuild.exe')
    fs.stat(msbuild, function (err, stat) {
      if (err) {
        if (err.code == 'ENOENT') {
          callback(new Error('Can\'t find "msbuild.exe". Do you have Microsoft Visual Studio C++ 2010 installed?'))
        } else {
          callback(err)
        }
        return
      }
      command = msbuild
      build()
    })
  }

  /**
   * Actually spawn the process and compile the module.
   */

  function build () {
    var buildType = config.target_defaults.default_configuration
      , platform = config.variables.target_arch == 'x64' ? '64' : '32'

    if (gyp.opts.debug) {
      buildType = 'Debug'
    }

    // Enable Verbose build
    if (!win && gyp.opts.verbose) {
      argv.push('V=1')
    }
    if (win && !gyp.opts.verbose) {
      argv.push('/clp:Verbosity=minimal')
    }

    // Turn off the Microsoft logo on Windows
    if (win) {
      argv.push('/nologo')
    }

    // Specify the build type, Release by default
    if (win) {
      argv.push('/p:Configuration=' + buildType + ';Platform=Win' + platform)
    } else {
      argv.push('BUILDTYPE=' + buildType)
      // Invoke the Makefile in the 'build' dir.
      argv.push('-C')
      argv.push('build')
    }

    if (win) {
      // did the user specify their own .sln file?
      var hasSln = argv.some(function (arg) {
        return path.extname(arg) == '.sln'
      })
      if (!hasSln) {
        argv.unshift(gyp.opts.solution || guessedSolution)
      }
    }

    asyncEmit(emitter, 'before', function (err) {
      if (err) return callback(err)
      var proc = gyp.spawn(command, argv)
      proc.on('exit', onExit)
    })
  }

  /**
   * Invoked after the make/msbuild command exits.
   */

  function onExit (code, signal) {
    asyncEmit(emitter, 'after', function (err) {
      if (err) return callback(err)
      if (code !== 0) {
        return callback(new Error('`' + command + '` failed with exit code: ' + code))
      }
      if (signal) {
        return callback(new Error('`' + command + '` got signal: ' + signal))
      }
      callback()
    })
  }

}
