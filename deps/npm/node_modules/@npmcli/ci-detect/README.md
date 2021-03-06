# @npmcli/ci-detect

Detect what kind of CI environment the program is in

[![Build Status](https://travis-ci.com/npm/ci-detect.svg?branch=master)](https://travis-ci.com/npm/ci-detect)
[![Coverage Status](https://coveralls.io/repos/github/npm/ci-detect/badge.svg?branch=master)](https://coveralls.io/github/npm/ci-detect?branch=master)

## USAGE

```js
const ciDetect = require('@npmcli/ci-detect')
// false if not in CI
// otherwise, a string indicating the CI environment type
const inCI = ciDetect()
```

## CIs Detected

Returns one of the following strings, or `false` if none match, by looking
at the appropriate environment variables.

* `'gerrit'` Gerrit
* `'gitlab'` GitLab
* `'circleci'` Circle-CI
* `'semaphore'` Semaphore
* `'drone'` Drone
* `'github-actions'` GitHub Actions
* `'tddium'` TDDium
* `'jenkins'` Jenkins
* `'bamboo'` Bamboo
* `'gocd'` GoCD
* `'wercker'` Oracle Wercker
* `'netlify'` Netlify
* `'now-github'` Zeit.co's Now for GitHub deployment service
* `'now-bitbucket'` Zeit.co's Now for BitBucket deployment service
* `'now-gitlab'` Zeit.co's Now for GitLab deployment service
* `'now'` Zeit.co's Now service, but not GitHub/BitBucket/GitLab
* `'azure-pipelines'` Azure Pipelines
* `'bitrise'` Bitrise
* `'buddy'` Buddy
* `'buildkite'` Buildkite
* `'cirrus'` Cirrus CI
* `'dsari'` dsari CI
* `'strider'` Strider CI
* `'taskcluster'` Mozilla Taskcluster
* `'hudson'` Hudson CI
* `'magnum'` Magnum CI
* `'nevercode'` Nevercode
* `'render'` Render CI
* `'sail'` Sail CI
* `'shippable'` Shippable
* `'heroku'` Heroku
* `'codeship'` CodeShip
* Anything that sets the `CI_NAME` environment variable will return the
  value as the result.  (This is how CodeShip is detected.)
* `'travis-ci'` Travis-CI - A few other CI systems set `TRAVIS=1` in the
  environment, because devs use that to indicate "test mode", so this one
  can get some false positives, and is tested later in the process to
  minimize this effect.
* `'aws-codebuild'` AWS CodeBuild
* `'builder'` Google Cloud Builder - This one is a bit weird.  It doesn't
  really set anything that can be reliably detected except
  `BUILDER_OUTPUT`, so it can get false positives pretty easily.
* `'custom'` anything else that sets `CI` environment variable to either
  `'1'` or `'true'`.

## Caveats

Note that since any program can set or unset whatever environment variables
they want, this is not 100% reliable.

Also, note that if your program does different behavior in
CI/test/deployment than other places, then there's a good chance that
you're doing something wrong!

But, for little niceties like setting colors or other output parameters, or
logging and that sort of non-essential thing, this module provides a way to
tweak without checking a bunch of things in a bunch of places.  Mostly,
it's a single place to keep a note of what CI system sets which environment
variable.
