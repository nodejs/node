# ci-info

Get details about the current Continuous Integration environment.

Please [open an
issue](https://github.com/watson/ci-info/issues/new?template=ci-server-not-detected.md)
if your CI server isn't properly detected :)

[![npm](https://img.shields.io/npm/v/ci-info.svg)](https://www.npmjs.com/package/ci-info)
[![Build status](https://travis-ci.org/watson/ci-info.svg?branch=master)](https://travis-ci.org/watson/ci-info)
[![js-standard-style](https://img.shields.io/badge/code%20style-standard-brightgreen.svg?style=flat)](https://github.com/feross/standard)

## Installation

```bash
npm install ci-info --save
```

## Usage

```js
var ci = require('ci-info')

if (ci.isCI) {
  console.log('The name of the CI server is:', ci.name)
} else {
  console.log('This program is not running on a CI server')
}
```

## Supported CI tools

Officially supported CI servers:

- [AWS CodeBuild](https://aws.amazon.com/codebuild/)
- [AppVeyor](http://www.appveyor.com)
- [Bamboo](https://www.atlassian.com/software/bamboo) by Atlassian
- [Bitbucket Pipelines](https://bitbucket.org/product/features/pipelines)
- [Buildkite](https://buildkite.com)
- [CircleCI](http://circleci.com)
- [Cirrus CI](https://cirrus-ci.org)
- [Codeship](https://codeship.com)
- [Drone](https://drone.io)
- [GitLab CI](https://about.gitlab.com/gitlab-ci/)
- [GoCD](https://www.go.cd/)
- [Hudson](http://hudson-ci.org)
- [Jenkins CI](https://jenkins-ci.org)
- [Magnum CI](https://magnum-ci.com)
- [Semaphore](https://semaphoreci.com)
- [Shippable](https://www.shippable.com/)
- [Solano CI](https://www.solanolabs.com/)
- [Strider CD](https://strider-cd.github.io/)
- [TaskCluster](http://docs.taskcluster.net)
- [Team Foundation Server](https://www.visualstudio.com/en-us/products/tfs-overview-vs.aspx) by Microsoft
- [TeamCity](https://www.jetbrains.com/teamcity/) by JetBrains
- [Travis CI](http://travis-ci.org)

## API

### `ci.name`

A string. Will contain the name of the CI server the code is running on.
If not CI server is detected, it will be `null`.

Don't depend on the value of this string not to change for a specific
vendor. If you find your self writing `ci.name === 'Travis CI'`, you
most likely want to use `ci.TRAVIS` instead.

### `ci.isCI`

A boolean. Will be `true` if the code is running on a CI server.
Otherwise `false`.

Some CI servers not listed here might still trigger the `ci.isCI`
boolean to be set to `true` if they use certain vendor neutral
environment variables. In those cases `ci.name` will be `null` and no
vendor specific boolean will be set to `true`.

### `ci.<VENDOR-CONSTANT>`

The following vendor specific boolean constants are exposed. A constant
will be `true` if the code is determined to run on the given CI server.
Otherwise `false`.

- `ci.APPVEYOR`
- `ci.BAMBOO`
- `ci.BITBUCKET`
- `ci.BUILDKITE`
- `ci.CIRCLE`
- `ci.CIRRUS`
- `ci.CODEBUILD`
- `ci.CODESHIP`
- `ci.DRONE`
- `ci.GITLAB`
- `ci.GOCD`
- `ci.HUDSON`
- `ci.JENKINS`
- `ci.MAGNUM`
- `ci.SEMAPHORE`
- `ci.SHIPPABLE`
- `ci.SOLANO`
- `ci.STRIDER`
- `ci.TASKCLUSTER`
- `ci.TEAMCITY`
- `ci.TFS` (Team Foundation Server)
- `ci.TRAVIS`

Deprecated vendor constants that will be removed in the next major
release:

- `ci.TDDIUM` (Solano CI) This have been renamed `ci.SOLANO`

## License

[MIT](LICENSE)
