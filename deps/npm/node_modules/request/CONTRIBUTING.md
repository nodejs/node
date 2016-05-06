
# Contributing to Request

:+1::tada: First off, thanks for taking the time to contribute! :tada::+1:

The following is a set of guidelines for contributing to Request and its packages, which are hosted in the [Request Organization](https://github.com/request) on GitHub.
These are just guidelines, not rules, use your best judgment and feel free to propose changes to this document in a pull request.


## Submitting an Issue

1. Provide a small self **sufficient** code example to **reproduce** the issue.
2. Run your test code using [request-debug](https://github.com/request/request-debug) and copy/paste the results inside the issue.
3. You should **always** use fenced code blocks when submitting code examples or any other formatted output:
  <pre>
  ```js
  put your javascript code here
  ```

  ```
  put any other formatted output here,
  like for example the one returned from using request-debug
  ```
  </pre>

If the problem cannot be reliably reproduced, the issue will be marked as `Not enough info (see CONTRIBUTING.md)`.

If the problem is not related to request the issue will be marked as `Help (please use Stackoverflow)`.


## Submitting a Pull Request

1. In almost all of the cases your PR **needs tests**. Make sure you have any.
2. Run `npm test` locally. Fix any errors before pushing to GitHub.
3. After submitting the PR a build will be triggered on TravisCI. Wait for it to ends and make sure all jobs are passing.


-----------------------------------------


## Becoming a Contributor

Individuals making significant and valuable contributions are given
commit-access to the project to contribute as they see fit. This project is
more like an open wiki than a standard guarded open source project.


## Rules

There are a few basic ground-rules for contributors:

1. **No `--force` pushes** or modifying the Git history in any way.
1. **Non-master branches** ought to be used for ongoing work.
1. **Any** change should be added through Pull Request.
1. **External API changes and significant modifications** ought to be subject
   to an **internal pull-request** to solicit feedback from other contributors.
1. Internal pull-requests to solicit feedback are *encouraged* for any other
   non-trivial contribution but left to the discretion of the contributor.
1. For significant changes wait a full 24 hours before merging so that active
   contributors who are distributed throughout the world have a chance to weigh
   in.
1. Contributors should attempt to adhere to the prevailing code-style.
1. Run `npm test` locally before submitting your PR, to catch any easy to miss
   style & testing issues.  To diagnose test failures, there are two ways to
   run a single test file:
     - `node_modules/.bin/taper tests/test-file.js` - run using the default
       [`taper`](https://github.com/nylen/taper) test reporter.
     - `node tests/test-file.js` - view the raw
       [tap](https://testanything.org/) output.


## Releases

Declaring formal releases remains the prerogative of the project maintainer.


## Changes to this arrangement

This is an experiment and feedback is welcome! This document may also be
subject to pull-requests or changes by contributors where you believe you have
something valuable to add or change.
