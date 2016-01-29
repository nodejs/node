'use strict'
var npm = require('../npm.js')
var util = require('util')
var nameValidator = require('validate-npm-package-name')

module.exports = errorMessage

function errorMessage (er) {
  var short = []
  var detail = []
  if (er.optional) {
    short.push(['optional', 'Skipping failed optional dependency ' + er.optional + ':'])
  }
  switch (er.code) {
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
      short.push(['', er])
      detail.push(['', ['\nPlease try running this command again as root/Administrator.'
                ].join('\n')])
      break

    case 'ELIFECYCLE':
      short.push(['', er.message])
      detail.push([
        '',
        [
          '',
          'Failed at the ' + er.pkgid + ' ' + er.stage + " script '" + er.script + "'.",
          'Make sure you have the latest version of node.js and npm installed.',
          'If you do, this is most likely a problem with the ' + er.pkgname + ' package,',
          'not with npm itself.',
          'Tell the author that this fails on your system:',
          '    ' + er.script,
          'You can get their info via:',
          '    npm owner ls ' + er.pkgname,
          'There is likely additional logging output above.'
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
          'This is most likely not a problem with npm itself.',
          'Please check if you have git installed and in your PATH.'
        ].join('\n')
      ])
      break

    case 'EJSONPARSE':
      short.push(['', er.message])
      short.push(['', 'File: ' + er.file])
      detail.push([
        '',
        [
          'Failed to parse package.json data.',
          'package.json must be actual JSON, not just JavaScript.',
          '',
          'This is not a bug in npm.',
          'Tell the package author to fix their package.json file.'
        ].join('\n'),
        'JSON.parse'
      ])
      break

    // TODO(isaacs)
    // Add a special case here for E401 and E403 explaining auth issues?

    case 'E404':
      // There's no need to have 404 in the message as well.
      var msg = er.message.replace(/^404\s+/, '')
      short.push(['404', msg])
      if (er.pkgid && er.pkgid !== '-') {
        detail.push(['404', ''])
        detail.push(['404', '', "'" + er.pkgid + "' is not in the npm registry."])

        var valResult = nameValidator(er.pkgid)

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
      short.push([
        'notsup',
        [
          'Not compatible with your operating system or architecture: ' + er.pkgid
        ].join('\n')
      ])
      detail.push([
        'notsup',
        [
          'Valid OS:    ' + (er.os.join ? er.os.join(',') : util.inspect(er.os)),
          'Valid Arch:  ' + (er.cpu.join ? er.cpu.join(',') : util.inspect(er.cpu)),
          'Actual OS:   ' + process.platform,
          'Actual Arch: ' + process.arch
        ].join('\n')
      ])
      break

    case 'EEXIST':
      short.push(['', er.message])
      short.push(['', 'File exists: ' + er.path])
      detail.push(['', 'Move it away, and try again.'])
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
          'This is most likely not a problem with npm itself',
          'and is related to network connectivity.',
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
          'This is most likely not a problem with npm itself.',
          "npm can't find a package.json file in your current directory."
        ].join('\n')
      ])
      break

    case 'ETARGET':
      short.push(['notarget', er.message])
      msg = [
        'This is most likely not a problem with npm itself.',
        'In most cases you or one of your dependencies are requesting',
        "a package version that doesn't exist."
      ]
      if (er.parent) {
        msg.push("\nIt was specified as a dependency of '" + er.parent + "'\n")
      }
      detail.push(['notarget', msg.join('\n')])
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
      /*eslint no-fallthrough:0*/

    case 'ENOSPC':
      short.push(['nospc', er.message])
      detail.push([
        'nospc',
        [
          'This is most likely not a problem with npm itself',
          'and is related to insufficient space on your system.'
        ].join('\n')
      ])
      break

    case 'EROFS':
      short.push(['rofs', er.message])
      detail.push([
        'rofs',
        [
          'This is most likely not a problem with npm itself',
          'and is related to the file system being read-only.',
          '\nOften virtualized file systems, or other file systems',
          "that don't support symlinks, give this error."
        ].join('\n')
      ])
      break

    case 'ENOENT':
      short.push(['enoent', er.message])
      detail.push([
        'enoent',
        [
          er.message,
          'This is most likely not a problem with npm itself',
          'and is related to npm not being able to find a file.',
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
          '    <http://github.com/npm/npm/issues>'
        ].join('\n')
      ])
      break

    case 'EISDIR':
      short.push(['eisdir', er.message])
      detail.push([
        'eisdir',
        [
          'This is most likely not a problem with npm itself',
          'and is related to npm not being able to find a package.json in',
          'a package you are trying to install.'
        ].join('\n')
      ])
      break

    default:
      short.push(['', er.message || er])
      detail.push([
        '',
        [
          '',
          'If you need help, you may report this error at:',
          '    <https://github.com/npm/npm/issues>'
        ].join('\n')
      ])
      break
  }
  return {summary: short, detail: detail}
}
