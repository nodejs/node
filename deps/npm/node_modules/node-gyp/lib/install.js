
module.exports = exports = install

exports.usage = 'Install node development files for the specified node version.'

/**
 * Module dependencies.
 */

var fs = require('fs')
  , tar = require('tar')
  , rm = require('rimraf')
  , path = require('path')
  , zlib = require('zlib')
  , mkdir = require('mkdirp')
  , semver = require('semver')
  , fstream = require('fstream')
  , request = require('request')
  , minimatch = require('minimatch')
  , distUrl = 'http://nodejs.org/dist'
  , win = process.platform == 'win32'

function install (gyp, argv, callback) {

  // ensure no double-callbacks happen
  function cb (err) {
    if (cb.done) return
    cb.done = true
    if (err) {
      gyp.verbose('got an error, rolling back install')
      // roll-back the install if anything went wrong
      gyp.commands.remove([ version ], function (err2) {
        callback(err)
      })
    } else {
      callback(null, version)
    }
  }


  // Determine which node dev files version we are installing
  var versionStr = argv[0] || gyp.opts.target || process.version
  gyp.verbose('input version string', versionStr)

  // parse the version to normalize and ensure it's valid
  var version = semver.parse(versionStr)
  if (!version) {
    return callback(new Error('Invalid version number: ' + versionStr))
  }

  // "legacy" versions are 0.7 and 0.6
  var isLegacy = semver.lt(versionStr, '0.8.0')
  gyp.verbose('installing legacy version?', isLegacy)

  if (semver.lt(versionStr, '0.6.0')) {
    return callback(new Error('Minimum target version is `0.6` or greater. Got: ' + versionStr))
  }

  // 0.x.y-pre versions are not published yet. Use previous release.
  if (version[5] === '-pre') {
    version[3] = +version[3] - 1
    version[5] = null
    gyp.verbose('-pre version detected, adjusting patch version')
  }

  // flatten version into String
  version = version.slice(1, 4).join('.')
  gyp.verbose('installing version', version)


  // TODO: Make ~/.node-gyp configurable
  var devDir = path.resolve(process.env.HOME, '.node-gyp', version)

  // If '--ensure' was passed, then don't *always* install the version,
  // check if it is already installed, and only install when needed
  if (gyp.opts.ensure) {
    gyp.verbose('--ensure was passed, so won\'t reinstall if already installed')
    fs.stat(devDir, function (err, stat) {
      if (err) {
        if (err.code == 'ENOENT') {
          gyp.verbose('version not already installed, continuing with install', version)
          go()
        } else {
          cb(err)
        }
        return
      }
      gyp.verbose('version is already installed, need to check "installVersion"')
      var installVersionFile = path.resolve(devDir, 'installVersion')
      fs.readFile(installVersionFile, 'ascii', function (err, ver) {
        if (err && err.code != 'ENOENT') {
          return cb(err)
        }
        var installVersion = parseInt(ver, 10) || 0
        gyp.verbose('got "installVersion":', installVersion)
        gyp.verbose('needs "installVersion":', gyp.package.installVersion)
        if (installVersion < gyp.package.installVersion) {
          gyp.verbose('version is no good; reinstalling')
          go()
        } else {
          gyp.verbose('version is good')
          cb()
        }
      })
    })
  } else {
    go()
  }

  function download(url,onError) {
    gyp.info('downloading:', url)
    var requestOpts = {
        uri: url
      , onResponse: true
    }

    // basic support for a proxy server
    var proxyUrl = gyp.opts.proxy
                || process.env.http_proxy
                || process.env.HTTP_PROXY
                || process.env.npm_config_proxy
    if (proxyUrl) {
      gyp.verbose('using proxy:', proxyUrl)
      requestOpts.proxy = proxyUrl
    }
    return request(requestOpts, onError)
  }

  function go () {

  // first create the dir for the node dev files
  mkdir(devDir, function (err) {
    if (err) return cb(err)

    // TODO: Detect if it was actually created or if it already existed
    gyp.verbose('created:', devDir)

    // now download the node tarball
    var tarballUrl = distUrl + '/v' + version + '/node-v' + version + '.tar.gz'
      , badDownload = false
      , extractCount = 0
      , gunzip = zlib.createGunzip()
      , extracter = tar.Extract({ path: devDir, strip: 1, filter: isValid })

    // checks if a file to be extracted from the tarball is valid.
    // only .h header files and the gyp files get extracted
    function isValid () {
      var name = this.path.substring(devDir.length + 1)
        , _valid = valid(name)
      if (name === '' && this.type === 'Directory') {
        // the first directory entry is ok
        return true
      }
      if (_valid) {
        gyp.verbose('extracted file from tarball', name)
        extractCount++
      } else {
        // invalid
      }
      return _valid
    }

    gunzip.on('error', cb)
    extracter.on('error', cb)
    extracter.on('end', afterTarball)

    // download the tarball, gunzip and extract!
    var req = download(tarballUrl, downloadError)
      .pipe(gunzip)
      .pipe(extracter)

    // something went wrong downloading the tarball?
    function downloadError (err, res) {
      if (err || res.statusCode != 200) {
        badDownload = true
        cb(err || new Error(res.statusCode + ' status code downloading tarball'))
      }
    }

    // invoked after the tarball has finished being extracted
    function afterTarball () {
      if (badDownload) return
      if (extractCount === 0) {
        return cb(new Error('There was a fatal problem while downloading/extracting the tarball'))
      }
      gyp.verbose('done parsing tarball')
      var async = 0

      if (isLegacy) {
        // copy over the files from the `legacy` dir
        async++
        copyLegacy(deref)
      }

      if (win) {
        // need to download node.lib
        async++
        downloadNodeLib(deref)
      }

      // write the "installVersion" file
      async++
      var installVersionPath = path.resolve(devDir, 'installVersion')
      fs.writeFile(installVersionPath, gyp.package.installVersion + '\n', deref)

      if (async === 0) {
        // no async tasks required
        cb()
      }

      function deref (err) {
        if (err) return cb(err)
        --async || cb()
      }
    }

    function copyLegacy (done) {
      // legacy versions of node (< 0.8) require the legacy files to be copied
      // over since they contain many bugfixes from the current node build system
      gyp.verbose('copying "legacy" gyp configuration files for version', version)

      var legacyDir = path.resolve(__dirname, '..', 'legacy')
      gyp.verbose('using "legacy" dir', legacyDir)
      gyp.verbose('copying to "dev" dir', devDir)

      var reader = fstream.Reader({ path: legacyDir, type: 'Directory' })
        , writer = fstream.Writer({ path: devDir, type: 'Directory' })

      reader.on('entry', function onEntry (entry) {
        gyp.verbose('reading entry', entry.path)
        entry.on('entry', onEntry)
      })

      reader.on('error', done)
      writer.on('error', done)

      // Like `cp -rpf`
      reader.pipe(writer)

      reader.on('end', done)
    }

    function downloadNodeLib (done) {
      gyp.verbose('on Windows; need to download `node.lib`...')
      // TODO: windows 64-bit support
      var releaseDir = path.resolve(devDir, 'Release')
        , debugDir = path.resolve(devDir, 'Debug')
        , nodeLibUrl = distUrl + '/v' + version + '/node.lib'

      gyp.verbose('Release dir', releaseDir)
      gyp.verbose('Debug dir', debugDir)
      gyp.verbose('`node.lib` url', nodeLibUrl)
      // TODO: parallelize mkdirs
      mkdir(releaseDir, function (err) {
        if (err) return done(err)
        mkdir(debugDir, function (err) {
          if (err) return done(err)
          // TODO: clean this mess up, written in a hastemode-9000
          var badDownload = false
          var res = download(nodeLibUrl, function (err, res) {
            if (err || res.statusCode != 200) {
              badDownload = true
              done(err || new Error(res.statusCode + ' status code downloading node.lib'))
            }
          })
          var releaseDirNodeLib = path.resolve(releaseDir, 'node.lib')
            , debugDirNodeLib = path.resolve(debugDir, 'node.lib')
            , rws = fs.createWriteStream(releaseDirNodeLib)
            , dws = fs.createWriteStream(debugDirNodeLib)
          gyp.verbose('streaming to', releaseDirNodeLib)
          gyp.verbose('streaming to', debugDirNodeLib)
          res.pipe(rws)
          res.pipe(dws)
          res.on('end', function () {
            if (badDownload) return
            done()
          })
        })
      })
    }


  })

  }

  /**
   * Checks if a given filename is "valid" for this installation.
   */

  function valid (file) {
      // header files
    return minimatch(file, '*.h', { matchBase: true })
      // non-legacy versions of node also extract the gyp build files
      || (!isLegacy &&
            (minimatch(file, '*.gypi', { matchBase: true })
          || minimatch(file, 'tools/gyp_addon')
          || (minimatch(file, 'tools/gyp/**') && !minimatch(file, 'tools/gyp/test/**'))
            )
         )
  }

}


install.trim = function trim (file) {
  var firstSlash = file.indexOf('/')
  return file.substring(firstSlash + 1)
}
