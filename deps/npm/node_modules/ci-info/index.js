'use strict'

var env = process.env

var vendors = [
  // Constant, Name, Envs
  ['TRAVIS', 'Travis CI', 'TRAVIS'],
  ['CIRCLE', 'CircleCI', 'CIRCLECI'],
  ['GITLAB', 'GitLab CI', 'GITLAB_CI'],
  ['APPVEYOR', 'AppVeyor', 'APPVEYOR'],
  ['CODESHIP', 'Codeship', {CI_NAME: 'codeship'}],
  ['DRONE', 'Drone', 'DRONE'],
  ['MAGNUM', 'Magnum CI', 'MAGNUM'],
  ['SEMAPHORE', 'Semaphore', 'SEMAPHORE'],
  ['JENKINS', 'Jenkins', 'JENKINS_URL', 'BUILD_ID'],
  ['BAMBOO', 'Bamboo', 'bamboo_planKey'],
  ['TFS', 'Team Foundation Server', 'TF_BUILD'],
  ['TEAMCITY', 'TeamCity', 'TEAMCITY_VERSION'],
  ['BUILDKITE', 'Buildkite', 'BUILDKITE'],
  ['HUDSON', 'Hudson', 'HUDSON_URL'],
  ['TASKCLUSTER', 'TaskCluster', 'TASK_ID', 'RUN_ID'],
  ['GOCD', 'GoCD', 'GO_PIPELINE_LABEL'],
  ['BITBUCKET', 'Bitbucket Pipelines', 'BITBUCKET_COMMIT'],
  ['CODEBUILD', 'AWS CodeBuild', 'CODEBUILD_BUILD_ARN']
]

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
  env.CI ||                      // Travis CI, CircleCI, Gitlab CI, Appveyor, CodeShip
  env.CONTINUOUS_INTEGRATION ||  // Travis CI
  env.BUILD_NUMBER ||            // Jenkins, TeamCity
  exports.name ||
  false
)
