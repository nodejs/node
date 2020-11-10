'use strict'

const summary = require('./install.js').summary
const Table = require('cli-table3')
const Utils = require('../lib/utils')

const report = function (data, options) {
  const defaults = {
    severityThreshold: 'info'
  }

  const blankChars = {
    'top': ' ',
    'top-mid': ' ',
    'top-left': ' ',
    'top-right': ' ',
    'bottom': ' ',
    'bottom-mid': ' ',
    'bottom-left': ' ',
    'bottom-right': ' ',
    'left': ' ',
    'left-mid': ' ',
    'mid': ' ',
    'mid-mid': ' ',
    'right': ' ',
    'right-mid': ' ',
    'middle': ' '
  }

  const config = Object.assign({}, defaults, options)

  let output = ''
  let exit = 0

  const log = function (value) {
    output = output + value + '\n'
  }

  const footer = function (data) {
    const total = Utils.totalVulnCount(data.metadata.vulnerabilities)

    if (total > 0) {
      exit = 1
    }
    log(`${summary(data, config)} in ${data.metadata.totalDependencies} scanned package${data.metadata.totalDependencies === 1 ? '' : 's'}`)
    if (total) {
      const counts = data.actions.reduce((acc, {action, isMajor, resolves}) => {
        if (action === 'update' || (action === 'install' && !isMajor)) {
          resolves.forEach(({id, path}) => acc.advisories.add(`${id}::${path}`))
        }
        if (isMajor) {
          resolves.forEach(({id, path}) => acc.major.add(`${id}::${path}`))
        }
        if (action === 'review') {
          resolves.forEach(({id, path}) => acc.review.add(`${id}::${path}`))
        }
        return acc
      }, {advisories: new Set(), major: new Set(), review: new Set()})
      if (counts.advisories.size) {
        log(`  run \`npm audit fix\` to fix ${counts.advisories.size} of them.`)
      }
      if (counts.major.size) {
        const maj = counts.major.size
        log(`  ${maj} vulnerabilit${maj === 1 ? 'y' : 'ies'} require${maj === 1 ? 's' : ''} semver-major dependency updates.`)
      }
      if (counts.review.size) {
        const rev = counts.review.size
        log(`  ${rev} vulnerabilit${rev === 1 ? 'y' : 'ies'} require${rev === 1 ? 's' : ''} manual review. See the full report for details.`)
      }
    }
  }

  const reportTitle = function () {
    const tableOptions = {
      colWidths: [78]
    }
    tableOptions.chars = blankChars
    const table = new Table(tableOptions)
    table.push([{
      content: '=== npm audit security report ===',
      vAlign: 'center',
      hAlign: 'center'
    }])
    log(table.toString())
  }

  const actions = function (data, config) {
    reportTitle()

    if (Object.keys(data.advisories).length !== 0) {
      // vulns found display a report.

      let reviewFlag = false

      data.actions.forEach((action) => {
        if (action.action === 'update' || action.action === 'install') {
          const recommendation = getRecommendation(action, config)
          const label = action.resolves.length === 1 ? 'vulnerability' : 'vulnerabilities'
          log(`# Run ${Utils.color(' ' + recommendation.cmd + ' ', 'inverse', config.withColor)} to resolve ${action.resolves.length} ${label}`)
          if (recommendation.isBreaking) {
            log(`SEMVER WARNING: Recommended action is a potentially breaking change`)
          }

          action.resolves.forEach((resolution) => {
            const advisory = data.advisories[resolution.id]
            const tableOptions = {
              colWidths: [15, 62],
              wordWrap: true
            }
            if (!config.withUnicode) {
              tableOptions.chars = blankChars
            }
            const table = new Table(tableOptions)

            table.push(
              {[Utils.severityLabel(advisory.severity, config.withColor, true)]: Utils.color(advisory.title, 'bold', config.withColor)},
              {'Package': advisory.module_name},
              {'Dependency of': `${resolution.path.split('>')[0]} ${resolution.dev ? '[dev]' : ''}`},
              {'Path': `${resolution.path.split('>').join(Utils.color(' > ', 'grey', config.withColor))}`},
              {'More info': advisory.url || `https://www.npmjs.com/advisories/${advisory.id}`}
            )

            log(table.toString() + '\n\n')
          })
        }
        if (action.action === 'review') {
          if (!reviewFlag) {
            const tableOptions = {
              colWidths: [78]
            }
            if (!config.withUnicode) {
              tableOptions.chars = blankChars
            }
            const table = new Table(tableOptions)
            table.push([{
              content: 'Manual Review\nSome vulnerabilities require your attention to resolve\n\nVisit https://go.npm.me/audit-guide for additional guidance',
              vAlign: 'center',
              hAlign: 'center'
            }])

            log(table.toString())
          }
          reviewFlag = true

          action.resolves.forEach((resolution) => {
            const advisory = data.advisories[resolution.id]
            const tableOptions = {
              colWidths: [15, 62],
              wordWrap: true
            }
            if (!config.withUnicode) {
              tableOptions.chars = blankChars
            }
            const table = new Table(tableOptions)
            const patchedIn = advisory.patched_versions.replace(' ', '') === '<0.0.0' ? 'No patch available' : advisory.patched_versions

            table.push(
              {[Utils.severityLabel(advisory.severity, config.withColor, true)]: Utils.color(advisory.title, 'bold', config.withColor)},
              {'Package': advisory.module_name},
              {'Patched in': patchedIn},
              {'Dependency of': `${resolution.path.split('>')[0]} ${resolution.dev ? '[dev]' : ''}`},
              {'Path': `${resolution.path.split('>').join(Utils.color(' > ', 'grey', config.withColor))}`},
              {'More info': advisory.url || `https://www.npmjs.com/advisories/${advisory.id}`}
            )
            log(table.toString())
          })
        }
      })
    }
  }

  actions(data, config)
  footer(data)

  return {
    report: output.trim(),
    exitCode: exit
  }
}

const getRecommendation = function (action, config) {
  if (action.action === 'install') {
    const isDev = action.resolves[0].dev

    return {
      cmd: `npm install ${isDev ? '--save-dev ' : ''}${action.module}@${action.target}`,
      isBreaking: action.isMajor
    }
  } else {
    return {
      cmd: `npm update ${action.module} --depth ${action.depth}`,
      isBreaking: false
    }
  }
}

module.exports = report
