var childProcess = require('child_process')
var escalade = require('escalade/sync')
var pico = require('picocolors')
var path = require('path')
var fs = require('fs')

var BrowserslistError = require('./error')

function detectLockfile () {
  var packageDir = escalade('.', function (dir, names) {
    return names.indexOf('package.json') !== -1 ? dir : ''
  })

  if (!packageDir) {
    throw new BrowserslistError(
      'Cannot find package.json. ' +
      'Is this the right directory to run `npx browserslist --update-db` in?'
    )
  }

  var lockfileNpm = path.join(packageDir, 'package-lock.json')
  var lockfileShrinkwrap = path.join(packageDir, 'npm-shrinkwrap.json')
  var lockfileYarn = path.join(packageDir, 'yarn.lock')
  var lockfilePnpm = path.join(packageDir, 'pnpm-lock.yaml')

  if (fs.existsSync(lockfilePnpm)) {
    return { mode: 'pnpm', file: lockfilePnpm }
  } else if (fs.existsSync(lockfileNpm)) {
    return { mode: 'npm', file: lockfileNpm }
  } else if (fs.existsSync(lockfileYarn)) {
    var lock = { mode: 'yarn', file: lockfileYarn }
    lock.content = fs.readFileSync(lock.file).toString()
    lock.version = /# yarn lockfile v1/.test(lock.content) ? 1 : 2
    return lock
  } else if (fs.existsSync(lockfileShrinkwrap)) {
    return { mode: 'npm', file: lockfileShrinkwrap }
  }
  throw new BrowserslistError(
    'No lockfile found. Run "npm install", "yarn install" or "pnpm install"'
  )
}

function getLatestInfo (lock) {
  if (lock.mode === 'yarn') {
    if (lock.version === 1) {
      return JSON.parse(
        childProcess.execSync('yarn info caniuse-lite --json').toString()
      ).data
    } else {
      return JSON.parse(
        childProcess.execSync('yarn npm info caniuse-lite --json').toString()
      )
    }
  }
  return JSON.parse(
    childProcess.execSync('npm show caniuse-lite --json').toString()
  )
}

function getBrowsersList () {
  return childProcess.execSync('npx browserslist').toString()
    .trim()
    .split('\n')
    .map(function (line) {
      return line.trim().split(' ')
    })
    .reduce(function (result, entry) {
      if (!result[entry[0]]) {
        result[entry[0]] = []
      }
      result[entry[0]].push(entry[1])
      return result
    }, {})
}

function diffBrowsersLists (old, current) {
  var browsers = Object.keys(old).concat(
    Object.keys(current).filter(function (browser) {
      return old[browser] === undefined
    })
  )
  return browsers.map(function (browser) {
    var oldVersions = old[browser] || []
    var currentVersions = current[browser] || []
    var intersection = oldVersions.filter(function (version) {
      return currentVersions.indexOf(version) !== -1
    })
    var addedVersions = currentVersions.filter(function (version) {
      return intersection.indexOf(version) === -1
    })
    var removedVersions = oldVersions.filter(function (version) {
      return intersection.indexOf(version) === -1
    })
    return removedVersions.map(function (version) {
      return pico.red('- ' + browser + ' ' + version)
    }).concat(addedVersions.map(function (version) {
      return pico.green('+ ' + browser + ' ' + version)
    }))
  })
    .reduce(function (result, array) {
      return result.concat(array)
    }, [])
    .join('\n')
}

function updateNpmLockfile (lock, latest) {
  var metadata = { latest: latest, versions: [] }
  var content = deletePackage(JSON.parse(lock.content), metadata)
  metadata.content = JSON.stringify(content, null, '  ')
  return metadata
}

function deletePackage (node, metadata) {
  if (node.dependencies) {
    if (node.dependencies['caniuse-lite']) {
      var version = node.dependencies['caniuse-lite'].version
      metadata.versions[version] = true
      delete node.dependencies['caniuse-lite']
    }
    for (var i in node.dependencies) {
      node.dependencies[i] = deletePackage(node.dependencies[i], metadata)
    }
  }
  return node
}

var yarnVersionRe = new RegExp('version "(.*?)"')

function updateYarnLockfile (lock, latest) {
  var blocks = lock.content.split(/(\n{2,})/).map(function (block) {
    return block.split('\n')
  })
  var versions = {}
  blocks.forEach(function (lines) {
    if (lines[0].indexOf('caniuse-lite@') !== -1) {
      var match = yarnVersionRe.exec(lines[1])
      versions[match[1]] = true
      if (match[1] !== latest.version) {
        lines[1] = lines[1].replace(
          /version "[^"]+"/, 'version "' + latest.version + '"'
        )
        lines[2] = lines[2].replace(
          /resolved "[^"]+"/, 'resolved "' + latest.dist.tarball + '"'
        )
        lines[3] = latest.dist.integrity ? lines[3].replace(
          /integrity .+/, 'integrity ' + latest.dist.integrity
        ) : ''
      }
    }
  })
  var content = blocks.map(function (lines) {
    return lines.join('\n')
  }).join('')
  return { content: content, versions: versions }
}

function updatePnpmLockfile (lock, latest) {
  var versions = {}
  var lines = lock.content.split('\n')
  var i
  var j
  var lineParts

  for (i = 0; i < lines.length; i++) {
    if (lines[i].indexOf('caniuse-lite:') >= 0) {
      lineParts = lines[i].split(/:\s?/, 2)
      versions[lineParts[1]] = true
      lines[i] = lineParts[0] + ': ' + latest.version
    } else if (lines[i].indexOf('/caniuse-lite') >= 0) {
      lineParts = lines[i].split(/([/:])/)
      for (j = 0; j < lineParts.length; j++) {
        if (lineParts[j].indexOf('caniuse-lite') >= 0) {
          versions[lineParts[j + 2]] = true
          lineParts[j + 2] = latest.version
          break
        }
      }
      lines[i] = lineParts.join('')
      for (i = i + 1; i < lines.length; i++) {
        if (lines[i].indexOf('integrity: ') !== -1) {
          lines[i] = lines[i].replace(
            /integrity: .+/, 'integrity: ' + latest.dist.integrity
          )
        } else if (lines[i].indexOf(' /') !== -1) {
          break
        }
      }
    }
  }
  return { content: lines.join('\n'), versions: versions }
}

function updateLockfile (lock, latest) {
  if (!lock.content) lock.content = fs.readFileSync(lock.file).toString()

  if (lock.mode === 'npm') {
    return updateNpmLockfile(lock, latest)
  } else if (lock.mode === 'yarn') {
    return updateYarnLockfile(lock, latest)
  }
  return updatePnpmLockfile(lock, latest)
}

function updatePackageManually (print, lock, latest) {
  var lockfileData = updateLockfile(lock, latest)
  var caniuseVersions = Object.keys(lockfileData.versions).sort()
  if (caniuseVersions.length === 1 &&
    caniuseVersions[0] === latest.version) {
    print(
      'Installed version:  ' + pico.bold(pico.green(latest.version)) + '\n' +
      pico.bold(pico.green('caniuse-lite is up to date')) + '\n'
    )
    return
  }

  if (caniuseVersions.length === 0) {
    caniuseVersions[0] = 'none'
  }
  print(
    'Installed version' +
    (caniuseVersions.length === 1 ? ':  ' : 's: ') +
    pico.bold(pico.red(caniuseVersions.join(', '))) +
    '\n' +
    'Removing old caniuse-lite from lock file\n'
  )
  fs.writeFileSync(lock.file, lockfileData.content)

  var install = lock.mode === 'yarn' ? 'yarn add -W' : lock.mode + ' install'
  print(
    'Installing new caniuse-lite version\n' +
    pico.yellow('$ ' + install + ' caniuse-lite') + '\n'
  )
  try {
    childProcess.execSync(install + ' caniuse-lite')
  } catch (e) /* istanbul ignore next */ {
    print(
      pico.red(
        '\n' +
        e.stack + '\n\n' +
        'Problem with `' + install + ' caniuse-lite` call. ' +
        'Run it manually.\n'
      )
    )
    process.exit(1)
  }

  var del = lock.mode === 'yarn' ? 'yarn remove -W' : lock.mode + ' uninstall'
  print(
    'Cleaning package.json dependencies from caniuse-lite\n' +
    pico.yellow('$ ' + del + ' caniuse-lite') + '\n'
  )
  childProcess.execSync(del + ' caniuse-lite')
}

module.exports = function updateDB (print) {
  var lock = detectLockfile()
  var latest = getLatestInfo(lock)

  var browsersListRetrievalError
  var oldBrowsersList
  try {
    oldBrowsersList = getBrowsersList()
  } catch (e) {
    browsersListRetrievalError = e
  }

  print(
    'Latest version:     ' + pico.bold(pico.green(latest.version)) + '\n'
  )

  if (lock.mode === 'yarn' && lock.version !== 1) {
    var update = 'yarn up -R'
    print(
      'Updating caniuse-lite version\n' +
      pico.yellow('$ ' + update + ' caniuse-lite') + '\n'
    )
    try {
      childProcess.execSync(update + ' caniuse-lite')
    } catch (e) /* istanbul ignore next */ {
      print(
        pico.red(
          '\n' +
          e.stack + '\n\n' +
          'Problem with `' + update + ' caniuse-lite` call. ' +
          'Run it manually.\n'
        )
      )
      process.exit(1)
    }
  } else {
    updatePackageManually(print, lock, latest)
  }

  print('caniuse-lite has been successfully updated\n')

  var currentBrowsersList
  if (!browsersListRetrievalError) {
    try {
      currentBrowsersList = getBrowsersList()
    } catch (e) /* istanbul ignore next */ {
      browsersListRetrievalError = e
    }
  }

  if (browsersListRetrievalError) {
    print(
      pico.red(
        '\n' +
        browsersListRetrievalError.stack + '\n\n' +
        'Problem with browser list retrieval.\n' +
        'Target browser changes won’t be shown.\n'
      )
    )
  } else {
    var targetBrowserChanges = diffBrowsersLists(
      oldBrowsersList,
      currentBrowsersList
    )
    if (targetBrowserChanges) {
      print('\nTarget browser changes:\n')
      print(targetBrowserChanges + '\n')
    } else {
      print('\n' + pico.green('No target browser changes') + '\n')
    }
  }
}
