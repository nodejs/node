'use strict'

const fs = require('graceful-fs')
const path = require('path')
const log = require('npmlog')
const os = require('os')
const processRelease = require('./process-release')
const win = process.platform === 'win32'
const findNodeDirectory = require('./find-node-directory')
const msgFormat = require('util').format
var findPython = require('./find-python')
if (win) {
  var findVisualStudio = require('./find-visualstudio')
}

function configure (gyp, argv, callback) {
  var python
  var buildDir = path.resolve('build')
  var configNames = ['config.gypi', 'common.gypi']
  var configs = []
  var nodeDir
  var release = processRelease(argv, gyp, process.version, process.release)

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
      gyp.opts.ensure = !gyp.opts.tarball

      gyp.commands.install([release.version], function (err) {
        if (err) {
          return callback(err)
        }
        log.verbose('get node dir', 'target node version installed:', release.versionDir)
        nodeDir = path.resolve(gyp.devDir, release.versionDir)
        createBuildDir()
      })
    }
  }

  function createBuildDir () {
    log.verbose('build dir', 'attempting to create "build" dir: %s', buildDir)
    fs.mkdir(buildDir, { recursive: true }, function (err, isNew) {
      if (err) {
        return callback(err)
      }
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
    if (err) {
      return callback(err)
    }

    var configFilename = 'config.gypi'
    var configPath = path.resolve(buildDir, configFilename)

    log.verbose('build/' + configFilename, 'creating config file')

    var config = process.config || {}
    var defaults = config.target_defaults
    var variables = config.variables

    // default "config.variables"
    if (!variables) {
      variables = config.variables = {}
    }

    // default "config.defaults"
    if (!defaults) {
      defaults = config.target_defaults = {}
    }

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
    if (variables.target_arch === 'arm64') {
      defaults.msvs_configuration_platform = 'ARM64'
      defaults.xcode_configuration_platform = 'arm64'
    }

    // set the node development directory
    variables.nodedir = nodeDir

    // disable -T "thin" static archives by default
    variables.standalone_static_library = gyp.opts.thin ? 0 : 1

    if (win) {
      process.env.GYP_MSVS_VERSION = Math.min(vsInfo.versionYear, 2015)
      process.env.GYP_MSVS_OVERRIDE_PATH = vsInfo.path
      defaults.msbuild_toolset = vsInfo.toolset
      if (vsInfo.sdk) {
        defaults.msvs_windows_target_platform_version = vsInfo.sdk
      }
      if (variables.target_arch === 'arm64') {
        if (vsInfo.versionMajor > 15 ||
            (vsInfo.versionMajor === 15 && vsInfo.versionMajor >= 9)) {
          defaults.msvs_enable_marmasm = 1
        } else {
          log.warn('Compiling ARM64 assembly is only available in\n' +
            'Visual Studio 2017 version 15.9 and above')
        }
      }
      variables.msbuild_path = vsInfo.msBuild
    }

    // loop through the rest of the opts and add the unknown ones as variables.
    // this allows for module-specific configure flags like:
    //
    //   $ node-gyp configure --shared-libxml2
    Object.keys(gyp.opts).forEach(function (opt) {
      if (opt === 'argv') {
        return
      }
      if (opt in gyp.configDefs) {
        return
      }
      variables[opt.replace(/-/g, '_')] = gyp.opts[opt]
    })

    // ensures that any boolean values from `process.config` get stringified
    function boolsToString (k, v) {
      if (typeof v === 'boolean') {
        return String(v)
      }
      return v
    }

    log.silly('build/' + configFilename, config)

    // now write out the config.gypi file to the build/ dir
    var prefix = '# Do not edit. File was generated by node-gyp\'s "configure" step'

    var json = JSON.stringify(config, boolsToString, 2)
    log.verbose('build/' + configFilename, 'writing out config file: %s', configPath)
    configs.push(configPath)
    fs.writeFile(configPath, [prefix, json, ''].join('\n'), findConfigs)
  }

  function findConfigs (err) {
    if (err) {
      return callback(err)
    }

    var name = configNames.shift()
    if (!name) {
      return runGyp()
    }
    var fullPath = path.resolve(name)

    log.verbose(name, 'checking for gypi file: %s', fullPath)
    fs.stat(fullPath, function (err) {
      if (err) {
        if (err.code === 'ENOENT') {
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
    if (err) {
      return callback(err)
    }

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
    var nodeExpFile
    if (process.platform === 'aix' || process.platform === 'os390') {
      var ext = process.platform === 'aix' ? 'exp' : 'x'
      var nodeRootDir = findNodeDirectory()
      var candidates

      if (process.platform === 'aix') {
        candidates = [
          'include/node/node',
          'out/Release/node',
          'out/Debug/node',
          'node'
        ].map(function (file) {
          return file + '.' + ext
        })
      } else {
        candidates = [
          'out/Release/obj.target/libnode',
          'out/Debug/obj.target/libnode',
          'lib/libnode'
        ].map(function (file) {
          return file + '.' + ext
        })
      }

      var logprefix = 'find exports file'
      nodeExpFile = findAccessibleSync(logprefix, nodeRootDir, candidates)
      if (nodeExpFile !== undefined) {
        log.verbose(logprefix, 'Found exports file: %s', nodeExpFile)
      } else {
        var msg = msgFormat('Could not find node.%s file in %s', ext, nodeRootDir)
        log.error(logprefix, 'Could not find exports file')
        return callback(new Error(msg))
      }
    }

    // this logic ported from the old `gyp_addon` python file
    var gypScript = path.resolve(__dirname, '..', 'gyp', 'gyp_main.py')
    var addonGypi = path.resolve(__dirname, '..', 'addon.gypi')
    var commonGypi = path.resolve(nodeDir, 'include/node/common.gypi')
    fs.stat(commonGypi, function (err) {
      if (err) {
        commonGypi = path.resolve(nodeDir, 'common.gypi')
      }

      var outputDir = 'build'
      if (win) {
        // Windows expects an absolute path
        outputDir = buildDir
      }
      var nodeGypDir = path.resolve(__dirname, '..')

      var nodeLibFile = path.join(nodeDir,
        !gyp.opts.nodedir ? '<(target_arch)' : '$(Configuration)',
        release.name + '.lib')

      argv.push('-I', addonGypi)
      argv.push('-I', commonGypi)
      argv.push('-Dlibrary=shared_library')
      argv.push('-Dvisibility=default')
      argv.push('-Dnode_root_dir=' + nodeDir)
      if (process.platform === 'aix' || process.platform === 'os390') {
        argv.push('-Dnode_exp_file=' + nodeExpFile)
      }
      argv.push('-Dnode_gyp_dir=' + nodeGypDir)

      // Do this to keep Cygwin environments happy, else the unescaped '\' gets eaten up,
      // resulting in bad paths, Ex c:parentFolderfolderanotherFolder instead of c:\parentFolder\folder\anotherFolder
      if (win) {
        nodeLibFile = nodeLibFile.replace(/\\/g, '\\\\')
      }
      argv.push('-Dnode_lib_file=' + nodeLibFile)
      argv.push('-Dmodule_root_dir=' + process.cwd())
      argv.push('-Dnode_engine=' +
        (gyp.opts.node_engine || process.jsEngine || 'v8'))
      argv.push('--depth=.')
      argv.push('--no-parallel')

      // tell gyp to write the Makefile/Solution files into output_dir
      argv.push('--generator-output', outputDir)

      // tell make to write its output into the same dir
      argv.push('-Goutput_dir=.')

      // enforce use of the "binding.gyp" file
      argv.unshift('binding.gyp')

      // execute `gyp` from the current target nodedir
      argv.unshift(gypScript)

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

module.exports = configure
module.exports.test = {
  findAccessibleSync: findAccessibleSync
}
module.exports.usage = 'Generates ' + (win ? 'MSVC project files' : 'a Makefile') + ' for the current module'
