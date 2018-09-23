'use strict'

var env = process.env

var vendors = [
  // Constant, Name, Envs
  ['APPVEYOR', 'AppVeyor', 'APPVEYOR'],
  ['BAMBOO', 'Bamboo', 'bamboo_planKey'],
  ['BITBUCKET', 'Bitbucket Pipelines', 'BITBUCKET_COMMIT'],
  ['BUILDKITE', 'Buildkite', 'BUILDKITE'],
  ['CIRCLE', 'CircleCI', 'CIRCLECI'],
  ['CIRRUS', 'Cirrus CI', 'CIRRUS_CI'],
  ['CODEBUILD', 'AWS CodeBuild', 'CODEBUILD_BUILD_ARN'],
  ['CODESHIP', 'Codeship', {CI_NAME: 'codeship'}],
  ['DRONE', 'Drone', 'DRONE'],
  ['GITLAB', 'GitLab CI', 'GITLAB_CI'],
  ['GOCD', 'GoCD', 'GO_PIPELINE_LABEL'],
  ['HUDSON', 'Hudson', 'HUDSON_URL'],
  ['JENKINS', 'Jenkins', 'JENKINS_URL', 'BUILD_ID'],
  ['MAGNUM', 'Magnum CI', 'MAGNUM'],
  ['SEMAPHORE', 'Semaphore', 'SEMAPHORE'],
  ['SHIPPABLE', 'Shippable', 'SHIPPABLE'],
  ['SOLANO', 'Solano CI', 'TDDIUM'],
  ['STRIDER', 'Strider CD', 'STRIDER'],
  ['TASKCLUSTER', 'TaskCluster', 'TASK_ID', 'RUN_ID'],
  ['TDDIUM', 'Solano CI', 'TDDIUM'], // Deprecated
  ['TEAMCITY', 'TeamCity', 'TEAMCITY_VERSION'],
  ['TFS', 'Team Foundation Server', 'TF_BUILD'],
  ['TRAVIS', 'Travis CI', 'TRAVIS']
]

// Used for testinging only
Object.defineProperty(exports, '_vendors', {
  value: vendors.map(function (v) { return v[0] })
})

exports.name = null

vendors.forEach(function (vendor) {
  var constant = vendor.shift()
  var name = vendor.shift()
  var isCI = vendor.every(function (obj) {
    if (typeof obj === 'string') return !!env[obj]
    return Object.keys(obj).every(function (k) {
      return env[k] === obj[k]
    })
  })
  exports[constant] = isCI
  if (isCI) exports.name = name
})

exports.isCI = !!(
  env.CI || // Travis CI, CircleCI, Cirrus CI, Gitlab CI, Appveyor, CodeShip
  env.CONTINUOUS_INTEGRATION || // Travis CI, Cirrus CI
  env.BUILD_NUMBER || // Jenkins, TeamCity
  exports.name ||
  false
)
