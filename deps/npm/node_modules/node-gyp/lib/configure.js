
module.exports = exports = configure

/**
 * Module dependencies.
 */

var fs = require('graceful-fs')
  , path = require('path')
  , glob = require('glob')
  , log = require('npmlog')
  , which = require('which')
  , semver = require('semver')
  , mkdirp = require('mkdirp')
  , exec = require('child_process').exec
  , win = process.platform == 'win32'

exports.usage = 'Generates ' + (win ? 'MSVC project files' : 'a Makefile') + ' for the current module'

function configure (gyp, argv, callback) {

  var python = process.env.PYTHON || gyp.opts.python || 'python'
    , buildDir = path.resolve('build')
    , configPath
    , nodeDir

  checkPython()

  // Make sure that Python is in the $PATH
  function checkPython () {
    log.verbose('check python', 'checking for Python executable "%s" in the PATH', python)
    which(python, function (err, execPath) {
      if (err) {
        if (win) {
          guessPython()
        } else {
          failNoPython()
        }
        return
      }
      log.verbose('`which` succeeded for `' + python + '`', execPath)
      checkPythonVersion()
    })
  }

  // Called on Windows when "python" isn't available in the current $PATH.
  // We're gonna glob C:\python2*
  function guessPython () {
    log.verbose('could not find "' + python + '". guessing location')
    var rootDir = process.env.SystemDrive || 'C:\\'
    if (rootDir[rootDir.length - 1] !== '\\') {
      rootDir += '\\'
    }
    var pythonPath = path.resolve(rootDir, 'Python27', 'python.exe')
    log.verbose('ensuring that file exists:', pythonPath)
    fs.stat(pythonPath, function (err, stat) {
      if (err) {
        if (err.code == 'ENOENT') {
          failNoPython()
        } else {
          callback(err)
        }
        return
      }
      python = pythonPath
      checkPythonVersion()
    })
  }

  function checkPythonVersion () {
    exec(python + ' --version', function (err, stdout, stderr) {
      if (err) {
        return callback(err)
      }
      log.verbose('check python version', '`python --version` returned: %j', stderr)
      var version = stderr.trim().replace(/[^\d\.]+/g, '')
      var numDots = 0
      version.replace(/\./g, function () {
        numDots++
      })
      while (numDots < 2) {
        version += '.0'
        numDots++
      }
      log.verbose('check python version', 'using version %j to check', version)
      if (semver.gte(version, '2.5.0') && semver.lt(version, '3.0.0')) {
        getNodeDir()
      } else {
        failPythonVersion(version)
      }
    })
  }

  function failNoPython () {
    callback(new Error('Can\'t find Python executable "' + python
          + '", you can set the PYTHON env variable.'))
  }

  function failPythonVersion (badVersion) {
    callback(new Error('Python executable "' + python
          + '" is v' + badVersion + ', which is not supported by gyp.\n'
          + 'You can pass the --python switch to point to Python >= v2.5.0 & < 3.0.0.'))
  }

  function getNodeDir () {

    // 'python' should be set by now
    process.env.PYTHON = python

    if (gyp.opts.nodedir) {
      // --nodedir was specified. use that for the dev files
      nodeDir = gyp.opts.nodedir

      // simple ~ homedir expansion based on https://github.com/joyent/node/issues/2857
      if (win) {
        nodeDir = nodeDir.replace(/^~/, process.env.USERPROFILE)
      } else {
        nodeDir = nodeDir.replace(/^~/, process.env.HOME)
      }

      log.verbose('get node dir', 'compiling against specified --nodedir dev files: %s', nodeDir)
      createBuildDir()

    } else {
      // if no --nodedir specified, ensure node dependencies are installed
      var version
      var versionStr

      if (gyp.opts.target) {
        // if --target was given, then determine a target version to compile for
        versionStr = gyp.opts.target
        log.verbose('get node dir', 'compiling against --target node version: %s', versionStr)
      } else {
        // if no --target was specified then use the current host node version
        versionStr = process.version
        log.verbose('get node dir', 'no --target version specified, falling back to host node version: %s', versionStr)
      }

      // make sure we have a valid version
      version = semver.parse(versionStr)
      if (!version) {
        return callback(new Error('Invalid version number: ' + versionStr))
      }

      // ensure that the target node version's dev files are installed
      gyp.opts.ensure = true
      gyp.commands.install([ versionStr ], function (err, version) {
        if (err) return callback(err)
        log.verbose('get node dir', 'target node version installed:', version)
        nodeDir = path.resolve(gyp.devDir, version)
        createBuildDir()
      })
    }
  }

  function createBuildDir () {
    log.verbose('build dir', 'attempting to create "build" dir: %s', buildDir)
    mkdirp(buildDir, function (err, isNew) {
      if (err) return callback(err)
      log.verbose('build dir', '"build" dir needed to be created?', isNew)
      createConfigFile()
    })
  }

  function createConfigFile (err) {
    if (err) return callback(err)

    var configFilename = 'config.gypi'
    configPath = path.resolve(buildDir, configFilename)

    log.verbose(configFilename, 'creating config file')

    var config = process.config || {}
      , defaults = config.target_defaults
      , variables = config.variables

    if (!defaults) {
      defaults = config.target_defaults = {}
    }
    if (!variables) {
      variables = config.variables = {}
    }
    if (!defaults.cflags) {
      defaults.cflags = []
    }
    if (!defaults.defines) {
      defaults.defines = []
    }
    if (!defaults.include_dirs) {
      defaults.include_dirs = []
    }
    if (!defaults.libraries) {
      defaults.libraries = []
    }

    // set the default_configuration prop
    if ('debug' in gyp.opts) {
      defaults.default_configuration = gyp.opts.debug ? 'Debug' : 'Release'
    }
    if (!defaults.default_configuration) {
      defaults.default_configuration = 'Release'
    }

    // set the target_arch variable
    variables.target_arch = gyp.opts.arch || process.arch || 'ia32'

    // set the node development directory
    variables.nodedir = nodeDir

    // don't copy dev libraries with nodedir option
    variables.copy_dev_lib = !gyp.opts.nodedir

    // loop through the rest of the opts and add the unknown ones as variables.
    // this allows for module-specific configure flags like:
    //
    //   $ node-gyp configure --shared-libxml2
    Object.keys(gyp.opts).forEach(function (opt) {
      if (opt === 'argv') return
      if (opt in gyp.configDefs) return
      variables[opt.replace(/-/g, '_')] = gyp.opts[opt]
    })

    // ensures that any boolean values from `process.config` get stringified
    function boolsToString (k, v) {
      if (typeof v === 'boolean')
        return String(v)
      return v
    }

    log.silly(configFilename, config)

    // now write out the config.gypi file to the build/ dir
    var prefix = '# Do not edit. File was generated by node-gyp\'s "configure" step'
      , json = JSON.stringify(config, boolsToString, 2)
    log.verbose(configFilename, 'writing out config file: %s', configPath)
    fs.writeFile(configPath, [prefix, json, ''].join('\n'), runGypAddon)
  }

  function runGypAddon (err) {
    if (err) return callback(err)

    // location of the `gyp_addon` python script for the target nodedir
    var gyp_addon = path.resolve(nodeDir, 'tools', 'gyp_addon')

    if (!~argv.indexOf('-f') && !~argv.indexOf('--format')) {
      if (win) {
        log.verbose('gyp_addon', 'gyp format was not specified; forcing "msvs"')
        // force the 'make' target for non-Windows
        argv.push('-f', 'msvs')
      } else {
        log.verbose('gyp_addon', 'gyp format was not specified; forcing "make"')
        // force the 'make' target for non-Windows
        argv.push('-f', 'make')
      }
    }

    function hasMsvsVersion () {
      return argv.some(function (arg) {
        return arg.indexOf('msvs_version') === 0
      })
    }

    if (win && !hasMsvsVersion()) {
      if ('msvs_version' in gyp.opts) {
        argv.push('-G', 'msvs_version=' + gyp.opts.msvs_version)
      } else {
        argv.push('-G', 'msvs_version=2010')
      }
    }

    // include the "config.gypi" file that was generated
    argv.unshift('-I' + configPath)

    // enforce use of the "binding.gyp" file
    argv.unshift('binding.gyp')

    // execute `gyp_addon` from the current target nodedir
    argv.unshift(gyp_addon)

    var cp = gyp.spawn(python, argv)
    cp.on('exit', onCpExit)
  }

  /**
   * Called when the `gyp_addon` child process exits.
   */

  function onCpExit (code, signal) {
    if (code !== 0) {
      callback(new Error('`gyp_addon` failed with exit code: ' + code))
    } else {
      // we're done
      callback()
    }
  }

}
