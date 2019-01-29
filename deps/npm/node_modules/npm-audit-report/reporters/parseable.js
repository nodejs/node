'use strict'

const report = function (data, options) {
  const defaults = {
    severityThreshold: 'info'
  }

  const config = Object.assign({}, defaults, options)

  let exit = 0

  const actions = function (data, config) {
    let accumulator = {
      critical: '',
      high: '',
      moderate: '',
      low: ''
    }

    if (Object.keys(data.advisories).length !== 0) {
      data.actions.forEach((action) => {
        let l = {}
        // Start with install/update actions
        if (action.action === 'update' || action.action === 'install') {
          const recommendation = getRecommendation(action, config)
          l.recommendation = recommendation.cmd
          l.breaking = recommendation.isBreaking ? 'Y' : 'N'

          action.resolves.forEach((resolution) => {
            const advisory = data.advisories[resolution.id]

            l.sevLevel = advisory.severity
            l.severity = advisory.title
            l.package = advisory.module_name
            l.moreInfo = advisory.url || `https://www.npmjs.com/advisories/${advisory.id}`
            l.path = resolution.path

            accumulator[advisory.severity] += [action.action, l.package, l.sevLevel, l.recommendation, l.severity, l.moreInfo, l.path, l.breaking]
              .join('\t') + '\n'
          }) // forEach resolves
        }

        if (action.action === 'review') {
          action.resolves.forEach((resolution) => {
            const advisory = data.advisories[resolution.id]

            l.sevLevel = advisory.severity
            l.severity = advisory.title
            l.package = advisory.module_name
            l.moreInfo = advisory.url || `https://www.npmjs.com/advisories/${advisory.id}`
            l.patchedIn = advisory.patched_versions.replace(' ', '') === '<0.0.0' ? 'No patch available' : advisory.patched_versions
            l.path = resolution.path

            accumulator[advisory.severity] += [action.action, l.package, l.sevLevel, l.patchedIn, l.severity, l.moreInfo, l.path].join('\t') + '\n'
          }) // forEach resolves
        } // is review
      }) // forEach actions
    }
    return accumulator['critical'] + accumulator['high'] + accumulator['moderate'] + accumulator['low']
  }

  const exitCode = function (metadata) {
    let total = 0
    const keys = Object.keys(metadata.vulnerabilities)
    for (let key of keys) {
      const value = metadata.vulnerabilities[key]
      total = total + value
    }

    if (total > 0) {
      exit = 1
    }
  }

  exitCode(data.metadata)

  return {
    report: actions(data, config),
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
