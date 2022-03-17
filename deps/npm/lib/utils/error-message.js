const { format } = require('util')
const { resolve } = require('path')
const nameValidator = require('validate-npm-package-name')
const replaceInfo = require('./replace-info.js')
const { report } = require('./explain-eresolve.js')
const log = require('./log-shim')

module.exports = (er, npm) => {
  const short = []
  const detail = []

  if (er.message) {
    er.message = replaceInfo(er.message)
  }
  if (er.stack) {
    er.stack = replaceInfo(er.stack)
  }

  switch (er.code) {
    case 'ERESOLVE':
      short.push(['ERESOLVE', er.message])
      detail.push(['', ''])
      // XXX(display): error messages are logged so we use the logColor since that is based
      // on stderr. This should be handled solely by the display layer so it could also be
      // printed to stdout if necessary.
      detail.push(['', report(er, !!npm.logColor, resolve(npm.cache, 'eresolve-report.txt'))])
      break

    case 'ENOLOCK': {
      const cmd = npm.command || ''
      short.push([cmd, 'This command requires an existing lockfile.'])
      detail.push([cmd, 'Try creating one first with: npm i --package-lock-only'])
      detail.push([cmd, `Original error: ${er.message}`])
      break
    }

    case 'ENOAUDIT':
      short.push(['audit', er.message])
      break

    case 'ECONNREFUSED':
      short.push(['', er])
      detail.push([
        '',
        [
          '\nIf you are behind a proxy, please make sure that the',
          "'proxy' config is set properly.  See: 'npm help config'",
        ].join('\n'),
      ])
      break

    case 'EACCES':
    case 'EPERM': {
      const isCachePath =
        typeof er.path === 'string' &&
        npm.config.loaded &&
        er.path.startsWith(npm.config.get('cache'))
      const isCacheDest =
        typeof er.dest === 'string' &&
        npm.config.loaded &&
        er.dest.startsWith(npm.config.get('cache'))

      const isWindows = require('./is-windows.js')

      if (!isWindows && (isCachePath || isCacheDest)) {
        // user probably doesn't need this, but still add it to the debug log
        log.verbose(er.stack)
        short.push([
          '',
          [
            '',
            'Your cache folder contains root-owned files, due to a bug in',
            'previous versions of npm which has since been addressed.',
            '',
            'To permanently fix this problem, please run:',
            `  sudo chown -R ${process.getuid()}:${process.getgid()} ${JSON.stringify(
              npm.config.get('cache')
            )}`,
          ].join('\n'),
        ])
      } else {
        short.push(['', er])
        detail.push([
          '',
          [
            '\nThe operation was rejected by your operating system.',
            isWindows
              /* eslint-disable-next-line max-len */
              ? "It's possible that the file was already in use (by a text editor or antivirus),\n" +
                'or that you lack permissions to access it.'
              /* eslint-disable-next-line max-len */
              : 'It is likely you do not have the permissions to access this file as the current user',
            '\nIf you believe this might be a permissions issue, please double-check the',
            'permissions of the file and its containing directories, or try running',
            'the command again as root/Administrator.',
          ].join('\n'),
        ])
      }
      break
    }

    case 'ENOGIT':
      short.push(['', er.message])
      detail.push([
        '',
        ['', 'Failed using git.', 'Please check if you have git installed and in your PATH.'].join(
          '\n'
        ),
      ])
      break

    case 'EJSONPARSE':
      // Check whether we ran into a conflict in our own package.json
      if (er.path === resolve(npm.prefix, 'package.json')) {
        const { isDiff } = require('parse-conflict-json')
        const txt = require('fs').readFileSync(er.path, 'utf8').replace(/\r\n/g, '\n')
        if (isDiff(txt)) {
          detail.push([
            '',
            [
              'Merge conflict detected in your package.json.',
              '',
              'Please resolve the package.json conflict and retry.',
            ].join('\n'),
          ])
          break
        }
      }
      short.push(['JSON.parse', er.message])
      detail.push([
        'JSON.parse',
        [
          'Failed to parse JSON data.',
          'Note: package.json must be actual JSON, not just JavaScript.',
        ].join('\n'),
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
            'it, or it timed out. Please try again.',
          ].join('\n'),
        ])
      } else {
        // npm ERR! code E401
        // npm ERR! Unable to authenticate, need: Basic
        const auth =
          !er.headers || !er.headers['www-authenticate']
            ? []
            : er.headers['www-authenticate'].map(au => au.split(/[,\s]+/))[0]

        if (auth.includes('Bearer')) {
          short.push([
            '',
            'Unable to authenticate, your authentication token seems to be invalid.',
          ])
          detail.push([
            '',
            ['To correct this please trying logging in again with:', '    npm login'].join('\n'),
          ])
        } else if (auth.includes('Basic')) {
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
              '    npm login',
            ].join('\n'),
          ])
        } else {
          short.push(['', er.message || er])
        }
      }
      break

    case 'E404':
      // There's no need to have 404 in the message as well.
      short.push(['404', er.message.replace(/^404\s+/, '')])
      if (er.pkgid && er.pkgid !== '-') {
        const pkg = er.pkgid.replace(/(?!^)@.*$/, '')

        detail.push(['404', ''])
        detail.push(['404', '', `'${replaceInfo(er.pkgid)}' is not in this registry.`])

        const valResult = nameValidator(pkg)

        if (!valResult.validForNewPackages) {
          detail.push(['404', 'This package name is not valid, because', ''])

          const errorsArray = [...(valResult.errors || []), ...(valResult.warnings || [])]
          errorsArray.forEach((item, idx) => detail.push(['404', ' ' + (idx + 1) + '. ' + item]))
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
        ['Refusing to remove it. Update manually,', 'or move it out of the way first.'].join('\n'),
      ])
      break

    case 'EBADPLATFORM': {
      const validOs =
        er.required && er.required.os && er.required.os.join
          ? er.required.os.join(',')
          : er.required.os
      const validArch =
        er.required && er.required.cpu && er.required.cpu.join
          ? er.required.cpu.join(',')
          : er.required.cpu
      const expected = { os: validOs, arch: validArch }
      const actual = { os: process.platform, arch: process.arch }
      short.push([
        'notsup',
        [
          format(
            'Unsupported platform for %s: wanted %j (current: %j)',
            er.pkgid,
            expected,
            actual
          ),
        ].join('\n'),
      ])
      detail.push([
        'notsup',
        [
          'Valid OS:    ' + validOs,
          'Valid Arch:  ' + validArch,
          'Actual OS:   ' + process.platform,
          'Actual Arch: ' + process.arch,
        ].join('\n'),
      ])
      break
    }

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
    case 'ERR_SOCKET_TIMEOUT':
    case 'EAI_FAIL':
      short.push(['network', er.message])
      detail.push([
        'network',
        [
          'This is a problem related to network connectivity.',
          'In most cases you are behind a proxy or have bad network settings.',
          '\nIf you are behind a proxy, please make sure that the',
          "'proxy' config is set properly.  See: 'npm help config'",
        ].join('\n'),
      ])
      break

    case 'ETARGET':
      short.push(['notarget', er.message])
      detail.push([
        'notarget',
        [
          'In most cases you or one of your dependencies are requesting',
          "a package version that doesn't exist.",
        ].join('\n'),
      ])
      break

    case 'E403':
      short.push(['403', er.message])
      detail.push([
        '403',
        [
          'In most cases, you or one of your dependencies are requesting',
          'a package version that is forbidden by your security policy, or',
          'on a server you do not have access to.',
        ].join('\n'),
      ])
      break

    case 'EBADENGINE':
      short.push(['engine', er.message])
      short.push(['engine', 'Not compatible with your version of node/npm: ' + er.pkgid])
      detail.push([
        'notsup',
        [
          'Not compatible with your version of node/npm: ' + er.pkgid,
          'Required: ' + JSON.stringify(er.required),
          'Actual:   ' +
            JSON.stringify({
              npm: npm.version,
              node: npm.config.loaded ? npm.config.get('node-version') : process.version,
            }),
        ].join('\n'),
      ])
      break

    case 'ENOSPC':
      short.push(['nospc', er.message])
      detail.push([
        'nospc',
        [
          'There appears to be insufficient space on your system to finish.',
          'Clear up some disk space and try again.',
        ].join('\n'),
      ])
      break

    case 'EROFS':
      short.push(['rofs', er.message])
      detail.push([
        'rofs',
        [
          'Often virtualized file systems, or other file systems',
          "that don't support symlinks, give this error.",
        ].join('\n'),
      ])
      break

    case 'ENOENT':
      short.push(['enoent', er.message])
      detail.push([
        'enoent',
        [
          'This is related to npm not being able to find a file.',
          er.file ? "\nCheck if the file '" + er.file + "' is present." : '',
        ].join('\n'),
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
          '    https://github.com/npm/cli/issues',
        ].join('\n'),
      ])
      break

    default:
      short.push(['', er.message || er])
      if (er.signal) {
        detail.push(['signal', er.signal])
      }

      if (er.cmd && Array.isArray(er.args)) {
        detail.push(['command', ...[er.cmd, ...er.args.map(replaceInfo)]])
      }

      if (er.stdout) {
        detail.push(['', er.stdout.trim()])
      }

      if (er.stderr) {
        detail.push(['', er.stderr.trim()])
      }

      break
  }
  return { summary: short, detail: detail }
}
