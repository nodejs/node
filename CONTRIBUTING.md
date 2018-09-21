# Contributing to Node.js

Contributions to Node.js include code, documentation, answering user questions,
running the project's infrastructure, and advocating for all types of Node.js
users.

The Node.js project welcomes all contributions from anyone willing to work in
good faith with other contributors and the community. No contribution is too
small and all contributions are valued.

This guide explains the process for contributing to the Node.js project's core
`nodejs/node` GitHub Repository and describes what to expect at each step.

## [Code of Conduct](./doc/guides/contributing/coc.md)

The Node.js project has a
[Code of Conduct](https://github.com/nodejs/admin/blob/master/CODE_OF_CONDUCT.md)
that *all* contributors are expected to follow. This code describes the
*minimum* behavior expectations for all contributors.

See [details on our policy on Code of Conduct](./doc/guides/contributing/coc.md).

## [Issues](./doc/guides/contributing/issues.md)

Issues in `nodejs/node` are the primary means by which bug reports and
general discussions are made.

* [How to Contribute in Issues](./doc/guides/contributing/issues.md#how-to-contribute-in-issues)
* [Asking for General Help](./doc/guides/contributing/issues.md#asking-for-general-help)
* [Discussing non-technical topics](./doc/guides/contributing/issues.md#discussing-non-technical-topics)
* [Submitting a Bug Report](./doc/guides/contributing/issues.md#submitting-a-bug-report)
* [Triaging a Bug Report](./doc/guides/contributing/issues.md#triaging-a-bug-report)
* [Resolving a Bug Report](./doc/guides/contributing/issues.md#resolving-a-bug-report)

## [Pull Requests](./doc/guides/contributing/pull-requests.md)

Pull Requests are the way concrete changes are made to the code, documentation,
dependencies, and tools contained in the `nodejs/node` repository.

* [Dependencies](./doc/guides/contributing/pull-requests.md#dependencies)
* [Setting up your local environment](./doc/guides/contributing/pull-requests.md#setting-up-your-local-environment)
* [The Process of Making Changes](./doc/guides/contributing/pull-requests.md#the-process-of-making-changes)
* [Reviewing Pull Requests](./doc/guides/contributing/pull-requests.md#reviewing-pull-requests)
* [Additional Notes](./doc/guides/contributing/pull-requests.md#additional-notes)

<<<<<<< HEAD
<a id="developers-certificate-of-origin"></a>
## Developer's Certificate of Origin 1.1
=======
#### Which branch?

For developing new features and bug fixes, the `master` branch should be pulled
and built upon.

#### Respect the stability index

The rules for the master branch are less strict; consult the
[stability index](./doc/api/documentation.markdown#stability-index) for details.

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
([IRC](http://webchat.freenode.net/?channels=io.js) is often the best medium.) Especially do so if you plan to work on something big. Nothing is more
frustrating than seeing your hard work go to waste because your vision
does not align with the project team.


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

```
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


### Step 4: Rebase

Use `git rebase` (not `git merge`) to sync your work from time to time.

```text
$ git fetch upstream
$ git rebase upstream/master
```


### Step 5: Test

Bug fixes and features **should come with tests**. Add your tests in the
test/parallel/ directory. Look at other tests to see how they should be
structured (license boilerplate, common includes, etc.).

```text
$ ./configure && make -j8 test
```

Make sure the linter is happy and that all tests pass. Please, do not submit
patches that fail either check.

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


## Developer's Certificate of Origin 1.0
>>>>>>> 30

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
