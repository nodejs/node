module.exports = exports = configure
module.exports.test = {
  PythonFinder: PythonFinder,
  findAccessibleSync: findAccessibleSync,
  findPython: findPython,
}

var fs = require('graceful-fs')
  , path = require('path')
  , log = require('npmlog')
  , os = require('os')
  , semver = require('semver')
  , mkdirp = require('mkdirp')
  , cp = require('child_process')
  , extend = require('util')._extend
  , processRelease = require('./process-release')
  , win = process.platform === 'win32'
  , findNodeDirectory = require('./find-node-directory')
  , msgFormat = require('util').format
  , logWithPrefix = require('./util').logWithPrefix
if (win)
  var findVisualStudio = require('./find-visualstudio')

exports.usage = 'Generates ' + (win ? 'MSVC project files' : 'a Makefile') + ' for the current module'

function configure (gyp, argv, callback) {
  var python
    , buildDir = path.resolve('build')
    , configNames = [ 'config.gypi', 'common.gypi' ]
    , configs = []
    , nodeDir
    , release = processRelease(argv, gyp, process.version, process.release)

  findPython(gyp.opts.python, function (err, found) {
    if (err) {
      callback(err)
    } else {
      python = found
      getNodeDir()
    }
  })

  function getNodeDir () {
    // 'python' should be set by now
    process.env.PYTHON = python

    if (gyp.opts.nodedir) {
      // --nodedir was specified. use that for the dev files
      nodeDir = gyp.opts.nodedir.replace(/^~/, os.homedir())

      log.verbose('get node dir', 'compiling against specified --nodedir dev files: %s', nodeDir)
      createBuildDir()
    } else {
      // if no --nodedir specified, ensure node dependencies are installed
      if ('v' + release.version !== process.version) {
        // if --target was given, then determine a target version to compile for
        log.verbose('get node dir', 'compiling against --target node version: %s', release.version)
      } else {
        // if no --target was specified then use the current host node version
        log.verbose('get node dir', 'no --target version specified, falling back to host node version: %s', release.version)
      }

      if (!release.semver) {
        // could not parse the version string with semver
        return callback(new Error('Invalid version number: ' + release.version))
      }

      // If the tarball option is set, always remove and reinstall the headers
      // into devdir. Otherwise only install if they're not already there.
      gyp.opts.ensure = gyp.opts.tarball ? false : true

      gyp.commands.install([ release.version ], function (err) {
        if (err) return callback(err)
        log.verbose('get node dir', 'target node version installed:', release.versionDir)
        nodeDir = path.resolve(gyp.devDir, release.versionDir)
        createBuildDir()
      })
    }
  }

  function createBuildDir () {
    log.verbose('build dir', 'attempting to create "build" dir: %s', buildDir)
    mkdirp(buildDir, function (err, isNew) {
      if (err) return callback(err)
      log.verbose('build dir', '"build" dir needed to be created?', isNew)
      if (win) {
        findVisualStudio(release.semver, gyp.opts.msvs_version,
          createConfigFile)
      } else {
        createConfigFile()
      }
    })
  }

  function createConfigFile (err, vsInfo) {
    if (err) return callback(err)

    var configFilename = 'config.gypi'
    var configPath = path.resolve(buildDir, configFilename)

    log.verbose('build/' + configFilename, 'creating config file')

    var config = process.config || {}
      , defaults = config.target_defaults
      , variables = config.variables

    // default "config.variables"
    if (!variables) variables = config.variables = {}

    // default "config.defaults"
    if (!defaults) defaults = config.target_defaults = {}

    // don't inherit the "defaults" from node's `process.config` object.
    // doing so could cause problems in cases where the `node` executable was
    // compiled on a different machine (with different lib/include paths) than
    // the machine where the addon is being built to
    defaults.cflags = []
    defaults.defines = []
    defaults.include_dirs = []
    defaults.libraries = []

    // set the default_configuration prop
    if ('debug' in gyp.opts) {
      defaults.default_configuration = gyp.opts.debug ? 'Debug' : 'Release'
    }
    if (!defaults.default_configuration) {
      defaults.default_configuration = 'Release'
    }

    // set the target_arch variable
    variables.target_arch = gyp.opts.arch || process.arch || 'ia32'
    if (variables.target_arch == 'arm64') {
      defaults['msvs_configuration_platform'] = 'ARM64'
    }

    // set the node development directory
    variables.nodedir = nodeDir

    // disable -T "thin" static archives by default
    variables.standalone_static_library = gyp.opts.thin ? 0 : 1

    if (win) {
      process.env['GYP_MSVS_VERSION'] = Math.min(vsInfo.versionYear, 2015)
      process.env['GYP_MSVS_OVERRIDE_PATH'] = vsInfo.path
      defaults['msbuild_toolset'] = vsInfo.toolset
      if (vsInfo.sdk) {
        defaults['msvs_windows_target_platform_version'] = vsInfo.sdk
      }
      if (variables.target_arch == 'arm64') {
        if (vsInfo.versionMajor > 15 ||
            (vsInfo.versionMajor === 15 && vsInfo.versionMajor >= 9)) {
          defaults['msvs_enable_marmasm'] = 1
        } else {
          log.warn('Compiling ARM64 assembly is only available in\n' +
            'Visual Studio 2017 version 15.9 and above')
        }
      }
      variables['msbuild_path'] = vsInfo.msBuild
    }

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

    log.silly('build/' + configFilename, config)

    // now write out the config.gypi file to the build/ dir
    var prefix = '# Do not edit. File was generated by node-gyp\'s "configure" step'
      , json = JSON.stringify(config, boolsToString, 2)
    log.verbose('build/' + configFilename, 'writing out config file: %s', configPath)
    configs.push(configPath)
    fs.writeFile(configPath, [prefix, json, ''].join('\n'), findConfigs)
  }

  function findConfigs (err) {
    if (err) return callback(err)
    var name = configNames.shift()
    if (!name) return runGyp()
    var fullPath = path.resolve(name)
    log.verbose(name, 'checking for gypi file: %s', fullPath)
    fs.stat(fullPath, function (err) {
      if (err) {
        if (err.code == 'ENOENT') {
          findConfigs() // check next gypi filename
        } else {
          callback(err)
        }
      } else {
        log.verbose(name, 'found gypi file')
        configs.push(fullPath)
        findConfigs()
      }
    })
  }

  function runGyp (err) {
    if (err) return callback(err)

    if (!~argv.indexOf('-f') && !~argv.indexOf('--format')) {
      if (win) {
        log.verbose('gyp', 'gyp format was not specified; forcing "msvs"')
        // force the 'make' target for non-Windows
        argv.push('-f', 'msvs')
      } else {
        log.verbose('gyp', 'gyp format was not specified; forcing "make"')
        // force the 'make' target for non-Windows
        argv.push('-f', 'make')
      }
    }

    // include all the ".gypi" files that were found
    configs.forEach(function (config) {
      argv.push('-I', config)
    })

    // For AIX and z/OS we need to set up the path to the exports file
    // which contains the symbols needed for linking.
    var node_exp_file = undefined
    if (process.platform === 'aix' || process.platform === 'os390') {
      var ext = process.platform === 'aix' ? 'exp' : 'x'
      var node_root_dir = findNodeDirectory()
      var candidates = undefined
      if (process.platform === 'aix') {
        candidates = ['include/node/node',
                      'out/Release/node',
                      'out/Debug/node',
                      'node'
                     ].map(function(file) {
                       return file + '.' + ext
                     })
      } else {
        candidates = ['out/Release/obj.target/libnode',
                      'out/Debug/obj.target/libnode',
                      'lib/libnode'
                     ].map(function(file) {
                       return file + '.' + ext
                     })
      }
      var logprefix = 'find exports file'
      node_exp_file = findAccessibleSync(logprefix, node_root_dir, candidates)
      if (node_exp_file !== undefined) {
        log.verbose(logprefix, 'Found exports file: %s', node_exp_file)
      } else {
        var msg = msgFormat('Could not find node.%s file in %s', ext, node_root_dir)
        log.error(logprefix, 'Could not find exports file')
        return callback(new Error(msg))
      }
    }

    // this logic ported from the old `gyp_addon` python file
    var gyp_script = path.resolve(__dirname, '..', 'gyp', 'gyp_main.py')
    var addon_gypi = path.resolve(__dirname, '..', 'addon.gypi')
    var common_gypi = path.resolve(nodeDir, 'include/node/common.gypi')
    fs.stat(common_gypi, function (err) {
      if (err)
        common_gypi = path.resolve(nodeDir, 'common.gypi')

      var output_dir = 'build'
      if (win) {
        // Windows expects an absolute path
        output_dir = buildDir
      }
      var nodeGypDir = path.resolve(__dirname, '..')
      var nodeLibFile = path.join(nodeDir,
        !gyp.opts.nodedir ? '<(target_arch)' : '$(Configuration)',
        release.name + '.lib')

      argv.push('-I', addon_gypi)
      argv.push('-I', common_gypi)
      argv.push('-Dlibrary=shared_library')
      argv.push('-Dvisibility=default')
      argv.push('-Dnode_root_dir=' + nodeDir)
      if (process.platform === 'aix' || process.platform === 'os390') {
        argv.push('-Dnode_exp_file=' + node_exp_file)
      }
      argv.push('-Dnode_gyp_dir=' + nodeGypDir)
      argv.push('-Dnode_lib_file=' + nodeLibFile)
      argv.push('-Dmodule_root_dir=' + process.cwd())
      argv.push('-Dnode_engine=' +
        (gyp.opts.node_engine || process.jsEngine || 'v8'))
      argv.push('--depth=.')
      argv.push('--no-parallel')

      // tell gyp to write the Makefile/Solution files into output_dir
      argv.push('--generator-output', output_dir)

      // tell make to write its output into the same dir
      argv.push('-Goutput_dir=.')

      // enforce use of the "binding.gyp" file
      argv.unshift('binding.gyp')

      // execute `gyp` from the current target nodedir
      argv.unshift(gyp_script)

      // make sure python uses files that came with this particular node package
      var pypath = [path.join(__dirname, '..', 'gyp', 'pylib')]
      if (process.env.PYTHONPATH) {
        pypath.push(process.env.PYTHONPATH)
      }
      process.env.PYTHONPATH = pypath.join(win ? ';' : ':')

      var cp = gyp.spawn(python, argv)
      cp.on('exit', onCpExit)
    })
  }

  function onCpExit (code) {
    if (code !== 0) {
      callback(new Error('`gyp` failed with exit code: ' + code))
    } else {
      // we're done
      callback()
    }
  }

}

/**
 * Returns the first file or directory from an array of candidates that is
 * readable by the current user, or undefined if none of the candidates are
 * readable.
 */
function findAccessibleSync (logprefix, dir, candidates) {
  for (var next = 0; next < candidates.length; next++) {
     var candidate = path.resolve(dir, candidates[next])
     try {
       var fd = fs.openSync(candidate, 'r')
     } catch (e) {
       // this candidate was not found or not readable, do nothing
       log.silly(logprefix, 'Could not open %s: %s', candidate, e.message)
       continue
     }
     fs.closeSync(fd)
     log.silly(logprefix, 'Found readable %s', candidate)
     return candidate
  }

  return undefined
}

function PythonFinder(configPython, callback) {
  this.callback = callback
  this.configPython = configPython
  this.errorLog = []
}

PythonFinder.prototype = {
  log: logWithPrefix(log, 'find Python'),
  argsExecutable: ['-c', 'import sys; print(sys.executable);'],
  argsVersion: ['-c', 'import sys; print("%s.%s.%s" % sys.version_info[:3]);'],
  semverRange: '>=2.6.0 <3.0.0',

  // These can be overridden for testing:
  execFile: cp.execFile,
  env: process.env,
  win: win,
  pyLauncher: 'py.exe',
  defaultLocation: path.join(process.env.SystemDrive || 'C:', 'Python27',
                             'python.exe'),

  // Logs a message at verbose level, but also saves it to be displayed later
  // at error level if an error occurs. This should help diagnose the problem.
  addLog: function addLog(message) {
    this.log.verbose(message)
    this.errorLog.push(message)
  },


  // Find Python by trying a sequence of possibilities.
  // Ignore errors, keep trying until Python is found.
  findPython: function findPython() {
    const SKIP=0, FAIL=1
    const toCheck = [
      {
        before: () => {
          if (!this.configPython) {
            this.addLog(
              'Python is not set from command line or npm configuration')
            return SKIP
          }
          this.addLog('checking Python explicitly set from command line or ' +
            'npm configuration')
          this.addLog('- "--python=" or "npm config get python" is ' +
            `"${this.configPython}"`)
        },
        check: this.checkCommand,
        arg: this.configPython,
      },
      {
        before: () => {
          if (!this.env.PYTHON) {
            this.addLog('Python is not set from environment variable PYTHON')
            return SKIP
          }
          this.addLog(
            'checking Python explicitly set from environment variable PYTHON')
          this.addLog(`- process.env.PYTHON is "${this.env.PYTHON}"`)
        },
        check: this.checkCommand,
        arg: this.env.PYTHON,
      },
      {
        before: () => { this.addLog('checking if "python2" can be used') },
        check: this.checkCommand,
        arg: 'python2',
      },
      {
        before: () => { this.addLog('checking if "python" can be used') },
        check: this.checkCommand,
        arg: 'python',
      },
      {
        before: () => {
          if (!this.win) {
            // Everything after this is Windows specific
            return FAIL
          }
          this.addLog(
            'checking if the py launcher can be used to find Python 2')
        },
        check: this.checkPyLauncher,
      },
      {
        before: () => {
          this.addLog(
            'checking if Python 2 is installed in the default location')
        },
        check: this.checkExecPath,
        arg: this.defaultLocation,
      },
    ]

    function runChecks(err) {
      this.log.silly('runChecks: err = %j', err && err.stack || err)

      const check = toCheck.shift()
      if (!check) {
        return this.fail()
      }

      const before = check.before.apply(this)
      if (before === SKIP) {
        return runChecks.apply(this)
      }
      if (before === FAIL) {
        return this.fail()
      }

      const args = [ runChecks.bind(this) ]
      if (check.arg) {
        args.unshift(check.arg)
      }
      check.check.apply(this, args)
    }

    runChecks.apply(this)
  },


  // Check if command is a valid Python to use.
  // Will exit the Python finder on success.
  // If on Windows, run in a CMD shell to support BAT/CMD launchers.
  checkCommand: function checkCommand (command, errorCallback) {
    var exec = command
    var args = this.argsExecutable
    var shell = false
    if (this.win) {
      // Arguments have to be manually quoted
      exec = `"${exec}"`
      args = args.map(a => `"${a}"`)
      shell = true
    }

    this.log.verbose(`- executing "${command}" to get executable path`)
    this.run(exec, args, shell, function (err, execPath) {
      // Possible outcomes:
      // - Error: not in PATH, not executable or execution fails
      // - Gibberish: the next command to check version will fail
      // - Absolute path to executable
      if (err) {
        this.addLog(`- "${command}" is not in PATH or produced an error`)
        return errorCallback(err)
      }
      this.addLog(`- executable path is "${execPath}"`)
      this.checkExecPath(execPath, errorCallback)
    }.bind(this))
  },

  // Check if the py launcher can find a valid Python to use.
  // Will exit the Python finder on success.
  // Distributions of Python on Windows by default install with the "py.exe"
  // Python launcher which is more likely to exist than the Python executable
  // being in the $PATH.
  // Because the Python launcher supports all versions of Python, we have to
  // explicitly request a Python 2 version. This is done by supplying "-2" as
  // the first command line argument. Since "py.exe -2" would be an invalid
  // executable for "execFile", we have to use the launcher to figure out
  // where the actual "python.exe" executable is located.
  checkPyLauncher: function checkPyLauncher (errorCallback) {
    this.log.verbose(
      `- executing "${this.pyLauncher}" to get Python 2 executable path`)
    this.run(this.pyLauncher, ['-2', ...this.argsExecutable], false,
             function (err, execPath) {
      // Possible outcomes: same as checkCommand
      if (err) {
        this.addLog(
          `- "${this.pyLauncher}" is not in PATH or produced an error`)
        return errorCallback(err)
      }
      this.addLog(`- executable path is "${execPath}"`)
      this.checkExecPath(execPath, errorCallback)
    }.bind(this))
  },

  // Check if a Python executable is the correct version to use.
  // Will exit the Python finder on success.
  checkExecPath: function checkExecPath (execPath, errorCallback) {
    this.log.verbose(`- executing "${execPath}" to get version`)
    this.run(execPath, this.argsVersion, false, function (err, version) {
      // Possible outcomes:
      // - Error: executable can not be run (likely meaning the command wasn't
      //   a Python executable and the previous command produced gibberish)
      // - Gibberish: somehow the last command produced an executable path,
      //   this will fail when verifying the version
      // - Version of the Python executable
      if (err) {
        this.addLog(`- "${execPath}" could not be run`)
        return errorCallback(err)
      }
      this.addLog(`- version is "${version}"`)

      const range = semver.Range(this.semverRange)
      var valid = false
      try {
        valid = range.test(version)
      } catch (err) {
        this.log.silly('range.test() threw:\n%s', err.stack)
        this.addLog(`- "${execPath}" does not have a valid version`)
        this.addLog('- is it a Python executable?')
        return errorCallback(err)
      }

      if (!valid) {
        this.addLog(`- version is ${version} - should be ${this.semverRange}`)
        this.addLog('- THIS VERSION OF PYTHON IS NOT SUPPORTED')
        return errorCallback(new Error(
          `Found unsupported Python version ${version}`))
      }
      this.succeed(execPath, version)
    }.bind(this))
  },

  // Run an executable or shell command, trimming the output.
  run: function run(exec, args, shell, callback) {
    var env = extend({}, this.env)
    env.TERM = 'dumb'
    const opts = { env: env, shell: shell }

    this.log.silly('execFile: exec = %j', exec)
    this.log.silly('execFile: args = %j', args)
    this.log.silly('execFile: opts = %j', opts)
    try {
      this.execFile(exec, args, opts, execFileCallback.bind(this))
    } catch (err) {
      this.log.silly('execFile: threw:\n%s', err.stack)
      return callback(err)
    }

    function execFileCallback(err, stdout, stderr) {
      this.log.silly('execFile result: err = %j', err && err.stack || err)
      this.log.silly('execFile result: stdout = %j', stdout)
      this.log.silly('execFile result: stderr = %j', stderr)
      if (err) {
        return callback(err)
      }
      const execPath = stdout.trim()
      callback(null, execPath)
    }
  },


  succeed: function succeed(execPath, version) {
    this.log.info(`using Python version ${version} found at "${execPath}"`)
    process.nextTick(this.callback.bind(null, null, execPath))
  },

  fail: function fail() {
    const errorLog = this.errorLog.join('\n')

    const pathExample = this.win ? 'C:\\Path\\To\\python.exe' :
                                   '/path/to/pythonexecutable'
    // For Windows 80 col console, use up to the column before the one marked
    // with X (total 79 chars including logger prefix, 58 chars usable here):
    //                                                           X
    const info = [
      '**********************************************************',
      'You need to install the latest version of Python 2.7.',
      'Node-gyp should be able to find and use Python. If not,',
      'you can try one of the following options:',
      `- Use the switch --python="${pathExample}"`,
      '  (accepted by both node-gyp and npm)',
      '- Set the environment variable PYTHON',
      '- Set the npm configuration variable python:',
      `  npm config set python "${pathExample}"`,
      'For more information consult the documentation at:',
      'https://github.com/nodejs/node-gyp#installation',
      '**********************************************************',
    ].join('\n')

    this.log.error(`\n${errorLog}\n\n${info}\n`)
    process.nextTick(this.callback.bind(null, new Error (
      'Could not find any Python 2 installation to use')))
  },
}

function findPython (configPython, callback) {
  var finder = new PythonFinder(configPython, callback)
  finder.findPython()
}
