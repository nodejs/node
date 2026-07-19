// pass in an arborist object, and it'll output the data about what
// was done, what was audited, etc.
//
// added ## packages, removed ## packages, and audited ## packages in 19.157s
//
// 1 package is looking for funding
//   run `npm fund` for details
//
// found 37 vulnerabilities (5 low, 7 moderate, 25 high)
//   run `npm audit fix` to fix them, or `npm audit` for details

const { log, output } = require('proc-log')
const { depth } = require('treeverse')
const ms = require('ms')
const npmAuditReport = require('npm-audit-report')
const { readTree: getFundingInfo } = require('libnpmfund')
const { trustedDisplay } = require('@npmcli/arborist/lib/script-allowed.js')
const auditError = require('./audit-error.js')
const { configSetAllowScripts } = require('./allow-scripts-remediation.js')

const reifyOutput = (npm, arb, extras = {}) => {
  const { diff, actualTree } = arb
  const unreviewedScripts = extras.unreviewedScripts || []

  // note: fails and crashes if we're running audit fix and there was an error which is a good thing, because there's no point printing all this other stuff in that case!
  const auditReport = auditError(npm, arb.auditReport) ? null : arb.auditReport

  // don't print any info in --silent mode, but we still need to set the exitCode properly from the audit report, if we have one.
  if (npm.silent) {
    getAuditReport(npm, auditReport)
    return
  }

  const summary = {
    add: [],
    added: 0,
    // audit gets added later
    audited: auditReport && !auditReport.error ? actualTree.inventory.size : 0,
    change: [],
    changed: 0,
    funding: 0,
    remove: [],
    removed: 0,
  }

  if (diff) {
    const showDiff = npm.config.get('dry-run') || npm.config.get('long')
    const chalk = npm.chalk

    depth({
      tree: diff,
      visit: d => {
        switch (d.action) {
          case 'REMOVE':
            if (showDiff) {
              output.standard(`${chalk.blue('remove')} ${d.actual.name} ${d.actual.package.version}`)
            }
            summary.removed++
            summary.remove.push({
              name: d.actual.name,
              version: d.actual.package.version,
              path: d.actual.path,
            })
            break
          case 'ADD':
            if (showDiff) {
              output.standard(`${chalk.green('add')} ${d.ideal.name} ${d.ideal.package.version}`)
            }
            // Linked store packages live under .store, absent from the logical actualTree, so identity lookup misses them; count each store package node (non-link).
            if (actualTree.inventory.has(d.ideal) || (d.ideal.isInStore && !d.ideal.isLink)) {
              summary.added++
              summary.add.push({
                name: d.ideal.name,
                version: d.ideal.package.version,
                path: d.ideal.path,
              })
            }
            break
          case 'CHANGE':
            if (showDiff) {
              output.standard(`${chalk.cyan('change')} ${d.actual.name} ${d.actual.package.version} => ${d.ideal.package.version}`)
            }
            summary.changed++
            summary.change.push({
              from: {
                name: d.actual.name,
                version: d.actual.package.version,
                path: d.actual.path,
              },
              to: {
                name: d.ideal.name,
                version: d.ideal.package.version,
                path: d.ideal.path,
              },
            })
            break
          default:
            return
        }
        const node = d.actual || d.ideal
        log.silly(d.action, node.location)
      },
      getChildren: d => d.children,
    })
  }

  if (npm.flatOptions.fund) {
    const fundingInfo = getFundingInfo(actualTree, { countOnly: true })
    summary.funding = fundingInfo.length
  }

  if (npm.flatOptions.json) {
    if (auditReport) {
      // call this to set the exit code properly
      getAuditReport(npm, auditReport)
      summary.audit = npm.command === 'audit' ? auditReport
        : auditReport.toJSON().metadata
    }
    if (unreviewedScripts.length) {
      summary.unreviewedScripts = unreviewedScripts.map(({ node, scripts }) => {
        const { name, version } = trustedDisplay(node)
        return {
          name,
          version,
          path: node.path,
          scripts,
        }
      })
    }
    output.buffer(summary)
  } else {
    packagesChangedMessage(npm, summary)
    packagesFundingMessage(npm, summary)
    printAuditReport(npm, auditReport)
    unreviewedScriptsMessage(npm, unreviewedScripts)
  }
}

// if we're running `npm audit fix`, then we print the full audit report at the end if there's still stuff, because it's silly for `npm audit` to tell you to run `npm audit` for details.
// otherwise, use the summary report.
// if we get here, we know it's not quiet or json.
// If the loglevel is silent, then we just run the report to get the exitCode set appropriately.
const printAuditReport = (npm, report) => {
  const res = getAuditReport(npm, report)
  if (!res || !res.report) {
    return
  }
  output.standard(`\n${res.report}`)
}

const getAuditReport = (npm, report) => {
  if (!report) {
    return
  }

  // when in silent mode, we print nothing.
  // the JSON output is going to just JSON.stringify() the report object.
  const reporter = npm.silent ? 'quiet'
    : npm.flatOptions.json ? 'quiet'
    : npm.command !== 'audit' ? 'install'
    : 'detail'
  const defaultAuditLevel = npm.command !== 'audit' ? 'none' : 'low'
  const auditLevel = npm.flatOptions.auditLevel || defaultAuditLevel

  const res = npmAuditReport(report, {
    reporter,
    ...npm.flatOptions,
    auditLevel,
    chalk: npm.chalk,
  })
  if (npm.command === 'audit') {
    process.exitCode = process.exitCode || res.exitCode
  }
  return res
}

const packagesChangedMessage = (npm, { added, removed, changed, audited }) => {
  const msg = ['\n']
  if (added === 0 && removed === 0 && changed === 0) {
    msg.push('up to date')
    if (audited) {
      msg.push(', ')
    }
  } else {
    if (added) {
      msg.push(`added ${added} package${added === 1 ? '' : 's'}`)
    }

    if (removed) {
      if (added) {
        msg.push(', ')
      }

      if (added && !audited && !changed) {
        msg.push('and ')
      }

      msg.push(`removed ${removed} package${removed === 1 ? '' : 's'}`)
    }
    if (changed) {
      if (added || removed) {
        msg.push(', ')
      }

      if (!audited && (added || removed)) {
        msg.push('and ')
      }

      msg.push(`changed ${changed} package${changed === 1 ? '' : 's'}`)
    }
    if (audited) {
      msg.push(', and ')
    }
  }
  if (audited) {
    msg.push(`audited ${audited} package${audited === 1 ? '' : 's'}`)
  }

  msg.push(` in ${ms(Date.now() - npm.started)}`)
  output.standard(msg.join(''))
}

const packagesFundingMessage = (npm, { funding }) => {
  if (!funding) {
    return
  }

  output.standard()
  const pkg = funding === 1 ? 'package' : 'packages'
  const is = funding === 1 ? 'is' : 'are'
  output.standard(`${funding} ${pkg} ${is} looking for funding`)
  output.standard('  run `npm fund` for details')
}

const unreviewedScriptsMessage = (npm, unreviewedScripts) => {
  if (!unreviewedScripts.length) {
    return
  }

  // Goes through log.warn so it respects --loglevel / --silent and lands
  // on stderr like every other "FYI, here's something to know" message.
  // stdout is reserved for things the user explicitly asked to see
  // (npm ls, npm view).
  const count = unreviewedScripts.length
  const pkg = count === 1 ? 'package has' : 'packages have'
  const header = `${count} ${pkg} install scripts not yet covered by allowScripts:`

  const names = []
  const lines = unreviewedScripts.map(({ node, scripts }) => {
    const { name, version } = trustedDisplay(node)
    /* istanbul ignore next: every test node has a name */
    const display = name || '<unknown>'
    names.push(display)
    const ver = version ? `@${version}` : ''
    const events = Object.entries(scripts)
      .map(([event, cmd]) => `${event}: ${cmd}`)
      .join('; ')
    return `  ${display}${ver} (${events})`
  })

  log.warn(
    'allow-scripts',
    [
      header,
      ...lines,
      '',
      ...remediationLines(npm, names),
    ].join('\n')
  )
}

// `npm install-scripts` writes to a project package.json, which doesn't
// exist for global installs (it throws EGLOBAL). For those, point users at
// the mechanism that does work globally: the `--allow-scripts` flag for a
// one-off, or `npm config set allow-scripts` to persist it.
const remediationLines = (npm, names) => {
  if (npm.global) {
    const list = names.join(',')
    return [
      `Run \`npm install -g --allow-scripts=${list}\` to allow these scripts ` +
      `once, or \`${configSetAllowScripts(names)}\` to allow them for ` +
      'all global installs.',
    ]
  }
  return [
    'Run `npm install-scripts ls` to review, ' +
    'or `npm install-scripts approve <pkg>` to allow.',
  ]
}

module.exports = reifyOutput
