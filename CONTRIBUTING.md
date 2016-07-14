# Contributing to Node.js

## Code of Conduct

The Code of Conduct explains the *bare minimum* behavior
expectations the Node Foundation requires of its contributors.
[Please read it before participating.](./CODE_OF_CONDUCT.md)

## Issue Contributions

When opening new issues or commenting on existing issues on this repository
please make sure discussions are related to concrete technical issues with the
Node.js software.

For general help using Node.js, please file an issue at the
[Node.js help repository](https://github.com/nodejs/help/issues).

Discussion of non-technical topics including subjects like intellectual
property, trademark and high level project questions should move to the
[Technical Steering Committee (TSC)](https://github.com/nodejs/TSC/issues)
instead.

## Code Contributions

The Node.js project has an open governance model and welcomes new contributors.
Individuals making significant and valuable contributions are made
_Collaborators_ and given commit-access to the project. See the
[GOVERNANCE.md](./GOVERNANCE.md) document for more information about how this
works.

This document will guide you through the contribution process.

### Step 1: Fork

Fork the project [on GitHub](https://github.com/nodejs/node) and check out your
copy locally.

```text
$ git clone git@github.com:username/node.git
$ cd node
$ git remote add upstream git://github.com/nodejs/node.git
```

#### Which branch?

For developing new features and bug fixes, the `master` branch should be pulled
and built upon.

#### Respect the stability index

The rules for the master branch are less strict; consult the
[stability index](./doc/api/documentation.md#stability-index) for details.

In a nutshell, modules are at varying levels of API stability. Bug fixes are
always welcome but API or behavioral changes to modules at stability level 3
(Locked) are off-limits.

#### Dependencies

Node.js has several bundled dependencies in the *deps/* and the *tools/*
directories that are not part of the project proper. Any changes to files
in those directories or its subdirectories should be sent to their respective
projects. Do not send your patch to us, we cannot accept it.

In case of doubt, open an issue in the
[issue tracker](https://github.com/nodejs/node/issues/) or contact one of the
[project Collaborators](https://github.com/nodejs/node/#current-project-team-members).
Especially do so if you plan to work on something big. Nothing is more
frustrating than seeing your hard work go to waste because your vision
does not align with the project team. Node.js has two IRC channels,
[#Node.js](http://webchat.freenode.net/?channels=node.js) for general help and questions, and
[#Node-dev](http://webchat.freenode.net/?channels=node-dev) for development of node core specifically.


### Step 2: Branch

Create a feature branch and start hacking:

```text
$ git checkout -b my-feature-branch -t origin/master
```

### Step 3: Commit

Make sure git knows your name and email address:

```text
$ git config --global user.name "J. Random User"
$ git config --global user.email "j.random.user@example.com"
```

Writing good commit logs is important. A commit log should describe what
changed and why. Follow these guidelines when writing one:

1. The first line should be 50 characters or less and contain a short
   description of the change prefixed with the name of the changed
   subsystem (e.g. "net: add localAddress and localPort to Socket").
2. Keep the second line blank.
3. Wrap all other lines at 72 columns.

A good commit log can look something like this:

```txt
subsystem: explaining the commit in one line

Body of commit message is a few lines of text, explaining things
in more detail, possibly giving some background about the issue
being fixed, etc. etc.

The body of the commit message can be several paragraphs, and
please do proper word-wrap and keep columns shorter than about
72 characters or so. That way `git log` will show things
nicely even when it is indented.
```

The header line should be meaningful; it is what other people see when they
run `git shortlog` or `git log --oneline`.

Check the output of `git log --oneline files_that_you_changed` to find out
what subsystem (or subsystems) your changes touch.

If your patch fixes an open issue, you can add a reference to it at the end
of the log. Use the `Fixes:` prefix and the full issue URL. For example:

```txt
Fixes: https://github.com/nodejs/node/issues/1337
```

### Step 4: Rebase

Use `git rebase` (not `git merge`) to sync your work from time to time.

```text
$ git fetch upstream
$ git rebase upstream/master
```

### Step 5: Test

Bug fixes and features **should come with tests**. Add your tests in the
`test/parallel/` directory. For guidance on how to write a test for the Node.js
project, see this [guide](./doc/guides/writing_tests.md). Looking at other tests
to see how they should be structured can also help.

To run the tests on Unix / OS X:

```text
$ ./configure && make -j8 test
```

Windows:

```text
> vcbuild test
```

(See the [BUILDING.md](./BUILDING.md) for more details.)

Make sure the linter is happy and that all tests pass. Please, do not submit
patches that fail either check.

Running `make test`/`vcbuild test` will run the linter as well unless one or
more tests fail.

If you want to run the linter without running tests, use
`make lint`/`vcbuild jslint`.

If you are updating tests and just want to run a single test to check it, you
can use this syntax to run it exactly as the test harness would:

```text
$ python tools/test.py -v --mode=release parallel/test-stream2-transform
```

You can run tests directly with node:

```text
$ ./node ./test/parallel/test-stream2-transform.js
```

Remember to recompile with `make -j8` in between test runs if you change
core modules.

### Step 6: Push

```text
$ git push origin my-feature-branch
```

Go to https://github.com/yourusername/node and select your feature branch.
Click the 'Pull Request' button and fill out the form.

Pull requests are usually reviewed within a few days. If there are comments
to address, apply your changes in a separate commit and push that to your
feature branch. Post a comment in the pull request afterwards; GitHub does
not send out notifications when you add commits.

<a id="developers-certificate-of-origin"></a>
## Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

* (a) The contribution was created in whole or in part by me and I
  have the right to submit it under the open source license
  indicated in the file; or

* (b) The contribution is based upon previous work that, to the best
  of my knowledge, is covered under an appropriate open source
  license and I have the right under that license to submit that
  work with modifications, whether created in whole or in part
  by me, under the same open source license (unless I am
  permitted to submit under a different license), as indicated
  in the file; or

* (c) The contribution was provided directly to me by some other
  person who certified (a), (b) or (c) and I have not modified
  it.

* (d) I understand and agree that this project and the contribution
  are public and that a record of the contribution (including all
  personal information I submit with it, including my sign-off) is
  maintained indefinitely and may be redistributed consistent with
  this project or the open source license(s) involved.
