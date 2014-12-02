# CONTRIBUTING

The node.js project welcomes new contributors.  This document will guide you
through the process.


### FORK

Fork the project [on GitHub](https://github.com/joyent/node) and check out
your copy.

```sh
$ git clone git@github.com:username/node.git
$ cd node
$ git remote add upstream git://github.com/joyent/node.git
```

Now decide if you want your feature or bug fix to go into the master branch
or the stable branch.  As a rule of thumb, bug fixes go into the stable branch
while new features go into the master branch.

The stable branch is effectively frozen; patches that change the node.js
API/ABI or affect the run-time behavior of applications get rejected.

The rules for the master branch are less strict; consult the
[stability index page][] for details.

In a nutshell, modules are at varying levels of API stability.  Bug fixes are
always welcome but API or behavioral  changes to modules at stability level 3
and up are off-limits.

Node.js has several bundled dependencies in the deps/ and the tools/
directories that are not part of the project proper.  Any changes to files
in those directories or its subdirectories should be sent to their respective
projects.  Do not send your patch to us, we cannot accept it.

In case of doubt, open an issue in the [issue tracker][], post your question
to the [node.js mailing list][] or contact one of the [project maintainers][]
on [IRC][].

Especially do so if you plan to work on something big.  Nothing is more
frustrating than seeing your hard work go to waste because your vision
does not align with that of a project maintainer.


### BRANCH

Okay, so you have decided on the proper branch.  Create a feature branch
and start hacking:

```sh
$ git checkout -b my-feature-branch -t origin/v0.10
```

(Where v0.10 is the latest stable branch as of this writing.)


### COMMIT

Make sure git knows your name and email address:

```sh
$ git config --global user.name "J. Random User"
$ git config --global user.email "j.random.user@example.com"
```

Writing good commit logs is important.  A commit log should describe what
changed and why.  Follow these guidelines when writing one:

1. The first line should be 50 characters or less and contain a short
   description of the change prefixed with the name of the changed
   subsystem (e.g. "net: add localAddress and localPort to Socket").
2. Keep the second line blank.
3. Wrap all other lines at 72 columns.

A good commit log looks like this:

```
subsystem: explaining the commit in one line

Body of commit message is a few lines of text, explaining things
in more detail, possibly giving some background about the issue
being fixed, etc etc.

The body of the commit message can be several paragraphs, and
please do proper word-wrap and keep columns shorter than about
72 characters or so. That way `git log` will show things
nicely even when it is indented.
```

The header line should be meaningful; it is what other people see when they
run `git shortlog` or `git log --oneline`.

Check the output of `git log --oneline files_that_you_changed` to find out
what subsystem (or subsystems) your changes touch.


### REBASE

Use `git rebase` (not `git merge`) to sync your work from time to time.

```sh
$ git fetch upstream
$ git rebase upstream/v0.10  # or upstream/master
```


### TEST

Bug fixes and features should come with tests.  Add your tests in the
test/simple/ directory.  Look at other tests to see how they should be
structured (license boilerplate, common includes, etc.).

```sh
$ make jslint test
```

Make sure the linter is happy and that all tests pass.  Please, do not submit
patches that fail either check.

If you are updating tests and just want to run a single test to check it, you
can use this syntax to run it exactly as the test harness would:

```
python tools/test.py -v --mode=release simple/test-stream2-transform
```

You can run tests directly with node:

```
node ./test/simple/test-streams2-transform.js
```


### PUSH

```sh
$ git push origin my-feature-branch
```

Go to https://github.com/username/node and select your feature branch.  Click
the 'Pull Request' button and fill out the form.

Pull requests are usually reviewed within a few days.  If there are comments
to address, apply your changes in a separate commit and push that to your
feature branch.  Post a comment in the pull request afterwards; GitHub does
not send out notifications when you add commits.


[stability index page]: https://github.com/joyent/node/blob/master/doc/api/documentation.markdown
[issue tracker]: https://github.com/joyent/node/issues
[node.js mailing list]: http://groups.google.com/group/nodejs
[IRC]: http://webchat.freenode.net/?channels=node.js
[project maintainers]: https://github.com/joyent/node/wiki/Project-Organization
# Contribution Policy

Individuals making significant and valuable contributions are given
commit-access to the project. These individuals are identified by the
Technical Committee (TC) and discussed during the weekly TC meeting.

If you make a significant contribution and are not considered for
commit-access log an issue and it will be brought up in the next TC
meeting.

Internal pull-requests to solicit feedback are required for any other
non-trivial contribution but left to the discretion of the
contributor.

Pull requests may be approved by any committer with sufficient
expertise to take full responsibility for the change, according to the
"Landing Patches" protocol described below.

## Landing Patches

- All bugfixes require a test case which demonstrates the defect.  The
  test should *fail* before the change, and *pass* after the change.
- Trivial changes (ie, those which fix bugs or improve performance
  without affecting API or causing other wide-reaching impact) may be
  landed immediately after review by a committer who did not write the
  code, provided that no other committers object to the change.
- If you are unsure, or if you are the author, have someone else
  review the change.
- For significant changes wait a full 48 hours (72 hours if it spans a
  weekend) before merging so that active contributors who are
  distributed throughout the world have a chance to weigh in.
- Controversial changes and **very** significant changes should not be
  merged until they have been discussed by the TC which will make any
  final decisions.
- Always include the `Reviewed-by: Your Name <your-email>` in the
  commit message.
- In commit messages also include `Fixes:` that either includes the
  **full url** (e.g.  `https://github.com/joyent/node/issues/...`),
  and/or the hash and commit message if the commit fixes a bug in a
  previous commit.
- PR's should include their full `PR-URL:` so it's easy to trace a
  commit back to the conversation that lead up to that change.
- Double check PR's to make sure the persons **full name** and email
  address are correct before merging.
- Except when updating dependencies, all commits should be self
  contained.  Meaning, every commit should pass all tests. This makes
  it much easier when bisecting to find a breaking change.
