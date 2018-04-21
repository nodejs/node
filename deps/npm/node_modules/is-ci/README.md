# is-ci

Returns `true` if the current environment is a Continuous Integration
server.

Please [open an issue](https://github.com/watson/is-ci/issues) if your
CI server isn't properly detected :)

[![Build status](https://travis-ci.org/watson/is-ci.svg?branch=master)](https://travis-ci.org/watson/is-ci)
[![js-standard-style](https://img.shields.io/badge/code%20style-standard-brightgreen.svg?style=flat)](https://github.com/feross/standard)

## Installation

```
npm install is-ci --save
```

## Programmatic Usage

```js
const isCI = require('is-ci')

if (isCI) {
  console.log('The code is running on a CI server')
}
```

## CLI Usage

For CLI usage you need to have the `is-ci` executable in your `PATH`.
There's a few ways to do that:

- Either install the module globally using `npm install is-ci -g`
- Or add the module as a dependency to your app in which case it can be
  used inside your package.json scripts as is
- Or provide the full path to the executable, e.g.
  `./node_modules/.bin/is-ci`

```
is-ci && echo "This is a CI server"
```

## Supported CI tools

Officially supported CI servers:

- [Travis CI](http://travis-ci.org)
- [CircleCI](http://circleci.com)
- [Jenkins CI](https://jenkins-ci.org)
- [Hudson](http://hudson-ci.org)
- [Bamboo](https://www.atlassian.com/software/bamboo)
- [TeamCity](https://www.jetbrains.com/teamcity/)
- [Team Foundation Server](https://www.visualstudio.com/en-us/products/tfs-overview-vs.aspx)
- [GitLab CI](https://about.gitlab.com/gitlab-ci/)
- [Codeship](https://codeship.com)
- [Drone.io](https://drone.io)
- [Magnum CI](https://magnum-ci.com)
- [Semaphore](https://semaphoreci.com)
- [AppVeyor](http://www.appveyor.com)
- [Buildkite](https://buildkite.com)
- [TaskCluster](http://docs.taskcluster.net)
- [GoCD](https://www.go.cd/)
- [Bitbucket Pipelines](https://bitbucket.org/product/features/pipelines)

Other CI tools using environment variables like `BUILD_ID` or `CI` would be detected as well.

## License

MIT
