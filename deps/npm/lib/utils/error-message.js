'use strict'
var npm = require('../npm.js')
var util = require('util')
var nameValidator = require('validate-npm-package-name')
var npmlog = require('npmlog')

module.exports = errorMessage

function errorMessage (er) {
  var short = []
  var detail = []
  switch (er.code) {
    case 'ENOAUDIT':
      short.push(['audit', er.message])
      break
    case 'EAUDITNOPJSON':
      short.push(['audit', er.message])
      break
    case 'EAUDITNOLOCK':
      short.push(['audit', er.message])
      detail.push(['audit', 'Try creating one first with: npm i --package-lock-only'])
      break

    case 'ECONNREFUSED':
      short.push(['', er])
      detail.push([
        '',
        [
          '\nIf you are behind a proxy, please make sure that the',
          "'proxy' config is set properly.  See: 'npm help config'"
        ].join('\n')
      ])
      break

    case 'EACCES':
    case 'EPERM':
      const isCachePath = typeof er.path === 'string' &&
        er.path.startsWith(npm.config.get('cache'))
      const isCacheDest = typeof er.dest === 'string' &&
        er.dest.startsWith(npm.config.get('cache'))

      const isWindows = process.platform === 'win32'

      if (!isWindows && (isCachePath || isCacheDest)) {
        // user probably doesn't need this, but still add it to the debug log
        npmlog.verbose(er.stack)
        short.push([
          '',
          [
            '',
            'Your cache folder contains root-owned files, due to a bug in',
            'previous versions of npm which has since been addressed.',
            '',
            'To permanently fix this problem, please run:',
            `  sudo chown -R ${process.getuid()}:${process.getgid()} ${JSON.stringify(npm.config.get('cache'))}`
          ].join('\n')
        ])
      } else {
        short.push(['', er])
        detail.push([
          '',
          [
            '\nThe operation was rejected by your operating system.',
            (process.platform === 'win32'
              ? 'It\'s possible that the file was already in use (by a text editor or antivirus),\n' +
                'or that you lack permissions to access it.'
              : 'It is likely you do not have the permissions to access this file as the current user'),
            '\nIf you believe this might be a permissions issue, please double-check the',
            'permissions of the file and its containing directories, or try running',
            'the command again as root/Administrator.'
          ].join('\n')])
      }
      break

    case 'EUIDLOOKUP':
      short.push(['lifecycle', er.message])
      detail.push([
        '',
        [
          '',
          'Failed to look up the user/group for running scripts.',
          '',
          'Try again with a different --user or --group settings, or',
          'run with --unsafe-perm to execute scripts as root.'
        ].join('\n')
      ])
      break

    case 'ELIFECYCLE':
      short.push(['', er.message])
      detail.push([
        '',
        [
          '',
          'Failed at the ' + er.pkgid + ' ' + er.stage + ' script.',
          'This is probably not a problem with npm. There is likely additional logging output above.'
        ].join('\n')]
      )
      break

    case 'ENOGIT':
      short.push(['', er.message])
      detail.push([
        '',
        [
          '',
          'Failed using git.',
          'Please check if you have git installed and in your PATH.'
        ].join('\n')
      ])
      break

    case 'EJSONPARSE':
      const path = require('path')
      // Check whether we ran into a conflict in our own package.json
      if (er.file === path.join(npm.prefix, 'package.json')) {
        const isDiff = require('../install/read-shrinkwrap.js')._isDiff
        const txt = require('fs').readFileSync(er.file, 'utf8')
        if (isDiff(txt)) {
          detail.push([
            '',
            [
              'Merge conflict detected in your package.json.',
              '',
              'Please resolve the package.json conflict and retry the command:',
              '',
              `$ ${process.argv.join(' ')}`
            ].join('\n')
          ])
          break
        }
      }
      short.push(['JSON.parse', er.message])
      detail.push([
        'JSON.parse',
        [
          'Failed to parse package.json data.',
          'package.json must be actual JSON, not just JavaScript.'
        ].join('\n')
      ])
      break

    case 'EOTP':
    case 'E401':
      // E401 is for places where we accidentally neglect OTP stuff
      if (er.code === 'EOTP' || /one-time pass/.test(er.message)) {
        short.push(['', 'This operation requires a one-time password from your authenticator.'])
        detail.push([
          '',
          [
            'You can provide a one-time password by passing --otp=<code> to the command you ran.',
            'If you already provided a one-time password then it is likely that you either typoed',
            'it, or it timed out. Please try again.'
          ].join('\n')
        ])
      } else {
        // npm ERR! code E401
        // npm ERR! Unable to authenticate, need: Basic
        const auth = (er.headers && er.headers['www-authenticate'] && er.headers['www-authenticate'].map((au) => au.split(/,\s*/))[0]) || []
        if (auth.indexOf('Bearer') !== -1) {
          short.push(['', 'Unable to authenticate, your authentication token seems to be invalid.'])
          detail.push([
            '',
            [
              'To correct this please trying logging in again with:',
              '    npm login'
            ].join('\n')
          ])
        } else if (auth.indexOf('Basic') !== -1) {
          short.push(['', 'Incorrect or missing password.'])
          detail.push([
            '',
            [
              'If you were trying to login, change your password, create an',
              'authentication token or enable two-factor authentication then',
              'that means you likely typed your password in incorrectly.',
              'Please try again, or recover your password at:',
              '    https://www.npmjs.com/forgot',
              '',
              'If you were doing some other operation then your saved credentials are',
              'probably out of date. To correct this please try logging in again with:',
              '    npm login'
            ].join('\n')
          ])
        } else {
          short.push(['', er.message || er])
        }
      }
      break

    case 'E404':
      // There's no need to have 404 in the message as well.
      var msg = er.message.replace(/^404\s+/, '')
      short.push(['404', msg])
      if (er.pkgid && er.pkgid !== '-') {
        var pkg = er.pkgid.replace(/(?!^)@.*$/, '')

        detail.push(['404', ''])
        detail.push(['404', '', "'" + er.pkgid + "' is not in the npm registry."])

        var valResult = nameValidator(pkg)

        if (valResult.validForNewPackages) {
          detail.push(['404', 'You should bug the author to publish it (or use the name yourself!)'])
        } else {
          detail.push(['404', 'Your package name is not valid, because', ''])

          var errorsArray = (valResult.errors || []).concat(valResult.warnings || [])
          errorsArray.forEach(function (item, idx) {
            detail.push(['404', ' ' + (idx + 1) + '. ' + item])
          })
        }

        if (er.parent) {
          detail.push(['404', "It was specified as a dependency of '" + er.parent + "'"])
        }
        detail.push(['404', '\nNote that you can also install from a'])
        detail.push(['404', 'tarball, folder, http url, or git url.'])
      }
      break

    case 'EPUBLISHCONFLICT':
      short.push(['publish fail', 'Cannot publish over existing version.'])
      detail.push(['publish fail', "Update the 'version' field in package.json and try again."])
      detail.push(['publish fail', ''])
      detail.push(['publish fail', 'To automatically increment version numbers, see:'])
      detail.push(['publish fail', '    npm help version'])
      break

    case 'EISGIT':
      short.push(['git', er.message])
      short.push(['git', '    ' + er.path])
      detail.push([
        'git',
        [
          'Refusing to remove it. Update manually,',
          'or move it out of the way first.'
        ].join('\n')
      ])
      break

    case 'ECYCLE':
      short.push([
        'cycle',
        [
          er.message,
          'While installing: ' + er.pkgid
        ].join('\n')
      ])
      detail.push([
        'cycle',
        [
          'Found a pathological dependency case that npm cannot solve.',
          'Please report this to the package author.'
        ].join('\n')
      ])
      break

    case 'EBADPLATFORM':
      var validOs = er.os.join ? er.os.join(',') : er.os
      var validArch = er.cpu.join ? er.cpu.join(',') : er.cpu
      var expected = {os: validOs, arch: validArch}
      var actual = {os: process.platform, arch: process.arch}
      short.push([
        'notsup',
        [
          util.format('Unsupported platform for %s: wanted %j (current: %j)', er.pkgid, expected, actual)
        ].join('\n')
      ])
      detail.push([
        'notsup',
        [
          'Valid OS:    ' + validOs,
          'Valid Arch:  ' + validArch,
          'Actual OS:   ' + process.platform,
          'Actual Arch: ' + process.arch
        ].join('\n')
      ])
      break

    case 'EEXIST':
      short.push(['', er.message])
      short.push(['', 'File exists: ' + (er.dest || er.path)])
      detail.push(['', 'Remove the existing file and try again, or run npm'])
      detail.push(['', 'with --force to overwrite files recklessly.'])
      break

    case 'ENEEDAUTH':
      short.push(['need auth', er.message])
      detail.push(['need auth', 'You need to authorize this machine using `npm adduser`'])
      break

    case 'ECONNRESET':
    case 'ENOTFOUND':
    case 'ETIMEDOUT':
    case 'EAI_FAIL':
      short.push(['network', er.message])
      detail.push([
        'network',
        [
          'This is a problem related to network connectivity.',
          'In most cases you are behind a proxy or have bad network settings.',
          '\nIf you are behind a proxy, please make sure that the',
          "'proxy' config is set properly.  See: 'npm help config'"
        ].join('\n')
      ])
      break

    case 'ENOPACKAGEJSON':
      short.push(['package.json', er.message])
      detail.push([
        'package.json',
        [
          "npm can't find a package.json file in your current directory."
        ].join('\n')
      ])
      break

    case 'ETARGET':
      short.push(['notarget', er.message])
      msg = [
        'In most cases you or one of your dependencies are requesting',
        "a package version that doesn't exist."
      ]
      if (er.parent) {
        msg.push("\nIt was specified as a dependency of '" + er.parent + "'\n")
      }
      detail.push(['notarget', msg.join('\n')])
      break

    case 'E403':
      short.push(['403', er.message])
      msg = [
        'In most cases, you or one of your dependencies are requesting',
        'a package version that is forbidden by your security policy.'
      ]
      if (er.parent) {
        msg.push("\nIt was specified as a dependency of '" + er.parent + "'\n")
      }
      detail.push(['403', msg.join('\n')])
      break

    case 'ENOTSUP':
      if (er.required) {
        short.push(['notsup', er.message])
        short.push(['notsup', 'Not compatible with your version of node/npm: ' + er.pkgid])
        detail.push([
          'notsup',
          [
            'Not compatible with your version of node/npm: ' + er.pkgid,
            'Required: ' + JSON.stringify(er.required),
            'Actual:   ' + JSON.stringify({
              npm: npm.version,
              node: npm.config.get('node-version')
            })
          ].join('\n')
        ])
        break
      } // else passthrough
      /* eslint no-fallthrough:0 */

    case 'ENOSPC':
      short.push(['nospc', er.message])
      detail.push([
        'nospc',
        [
          'There appears to be insufficient space on your system to finish.',
          'Clear up some disk space and try again.'
        ].join('\n')
      ])
      break

    case 'EROFS':
      short.push(['rofs', er.message])
      detail.push([
        'rofs',
        [
          'Often virtualized file systems, or other file systems',
          "that don't support symlinks, give this error."
        ].join('\n')
      ])
      break

    case 'ENOENT':
      short.push(['enoent', er.message])
      detail.push([
        'enoent',
        [
          'This is related to npm not being able to find a file.',
          er.file ? "\nCheck if the file '" + er.file + "' is present." : ''
        ].join('\n')
      ])
      break

    case 'EMISSINGARG':
    case 'EUNKNOWNTYPE':
    case 'EINVALIDTYPE':
    case 'ETOOMANYARGS':
      short.push(['typeerror', er.stack])
      detail.push([
        'typeerror',
        [
          'This is an error with npm itself. Please report this error at:',
          '    <https://npm.community>'
        ].join('\n')
      ])
      break

    default:
      short.push(['', er.message || er])
      break
  }
  if (er.optional) {
    short.unshift(['optional', er.optional + ' (' + er.location + '):'])
    short.concat(detail).forEach(function (msg) {
      if (!msg[0]) msg[0] = 'optional'
      if (msg[1]) msg[1] = msg[1].toString().replace(/(^|\n)/g, '$1SKIPPING OPTIONAL DEPENDENCY: ')
    })
  }
  return {summary: short, detail: detail}
}
