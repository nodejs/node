# CONTRIBUTING

## ISSUE CONTRIBUTIONS

When opening new issues or commenting on existing issues on this repository
please make sure discussions are related to concrete technical issues with the
`iojs` software.

Discussion of non-technical topics including subjects like intellectual
property, trademark and high level project questions should move to the
[node-forward discussion repository][] instead.

## CODE CONTRIBUTIONS

The io.js project welcomes new contributors.  This document will guide you
through the process.


### FORK

Fork the project [on GitHub](https://github.com/iojs/io.js) and check out
your copy.

```sh
$ git clone git@github.com:username/io.js.git
$ cd io.js
$ git remote add upstream git://github.com/iojs/io.js.git
```

Now decide if you want your feature or bug fix to go into the master branch
or the stable branch.  As a rule of thumb, bug fixes go into the stable branch
while new features go into the master branch.

The stable branch is effectively frozen; patches that change the io.js
API/ABI or affect the run-time behavior of applications get rejected.

The rules for the master branch are less strict; consult the
[stability index page][] for details.

In a nutshell, modules are at varying levels of API stability.  Bug fixes are
always welcome but API or behavioral  changes to modules at stability level 3
and up are off-limits.

io.js has several bundled dependencies in the deps/ and the tools/
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
$ git checkout -b my-feature-branch -t origin/v0.12
```

(Where v0.12 is the latest stable branch as of this writing.)


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
$ git rebase upstream/v0.12  # or upstream/master
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

Go to https://github.com/username/io.js and select your feature branch.  Click
the 'Pull Request' button and fill out the form.

Pull requests are usually reviewed within a few days.  If there are comments
to address, apply your changes in a separate commit and push that to your
feature branch.  Post a comment in the pull request afterwards; GitHub does
not send out notifications when you add commits.


[stability index page]: https://github.com/joyent/node/blob/master/doc/api/documentation.markdown
[issue tracker]: https://github.com/joyent/node/issues
[node.js mailing list]: http://groups.google.com/group/nodejs
[IRC]: http://webchat.freenode.net/?channels=io.js
[project maintainers]: https://github.com/joyent/node/wiki/Project-Organization
[node-forward discussion repository]: https://github.com/node-forward/discussions/issues

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
  **full url** (e.g.  `https://github.com/iojs/io.js/issues/...`),
  and/or the hash and commit message if the commit fixes a bug in a
  previous commit.
- PR's should include their full `PR-URL:` so it's easy to trace a
  commit back to the conversation that lead up to that change.
- Double check PR's to make sure the person's **full name** and email
  address are correct before merging.
- Except when updating dependencies, all commits should be self
  contained.  Meaning, every commit should pass all tests. This makes
  it much easier when bisecting to find a breaking change.

### Direct instruction

(Optional) Ensure that you are not in a borked `am`/`rebase` state

```sh
git am --abort
git rebase --abort
```

Checkout proper target branch

```sh
git checkout v0.12
```

Update the tree

```sh
git fetch origin
git merge --ff-only origin/v0.12
```

Apply external patches

```sh
curl https://github.com/iojs/io.js/pull/xxx.patch | git am --whitespace=fix
```

Check and re-review the changes

```sh
git diff origin/v0.12
```

Check number of commits and commit messages

```sh
git log origin/v0.12...v0.12
```

If there are multiple commits that relate to the same feature or
one with a feature and separate with a test for that feature -
you'll need to squash them (or strictly speaking `fixup`).

```sh
git rebase -i origin/v0.12
```

This will open a screen like this (in the default shell editor):

```sh
pick 6928fc1 crypto: add feature A
pick 8120c4c add test for feature A
pick 51759dc feature B
pick 7d6f433 test for feature B

# Rebase f9456a2..7d6f433 onto f9456a2
#
# Commands:
#  p, pick = use commit
#  r, reword = use commit, but edit the commit message
#  e, edit = use commit, but stop for amending
#  s, squash = use commit, but meld into previous commit
#  f, fixup = like "squash", but discard this commit's log message
#  x, exec = run command (the rest of the line) using shell
#
# These lines can be re-ordered; they are executed from top to bottom.
#
# If you remove a line here THAT COMMIT WILL BE LOST.
#
# However, if you remove everything, the rebase will be aborted.
#
# Note that empty commits are commented out
```

Replace a couple of `pick`s with `fixup` to squash them into a previous commit:

```sh
pick 6928fc1 crypto: add feature A
fixup 8120c4c add test for feature A
pick 51759dc feature B
fixup 7d6f433 test for feature B
```

Replace `pick` with `reword` to change the commit message:

```sh
reword 6928fc1 crypto: add feature A
fixup 8120c4c add test for feature A
reword 51759dc feature B
fixup 7d6f433 test for feature B
```

Save the file and close the editor, you'll be asked to enter new commit message
for that commit, and everything else should go smoothly. Note that this is a
good moment to fix incorrect commit logs, ensure that they are properly
formatted, and add `Reviewed-By` line.

Time to push it:

```sh
git push origin v0.12
```

# Governance

This repository is jointly governed by a technical committee, commonly
referred to as the "TC."

The TC has final authority over this project including:

* Technical direction
* Project governance and process (including this policy)
* Contribution policy
* GitHub repository hosting
* Conduct guidelines

## Membership

Initial membership invitations to the TC were given to individuals who
had been active contributors to io.js, and who have significant
experience with the management of the io.js project.  Membership is
expected to evolve over time according to the needs of the project.

Current membership is:

```
Ben Noordhuis (@bnoordhuis)
Bert Belder (@piscisaureus)
Fedor Indutny (@indutny)
Isaac Z. Schlueter (@isaacs)
Nathan Rajlich (@TooTallNate)
TJ Fontaine (@tjfontaine)
Trevor Norris (@trevnorris)
```

TC seats are not time-limited.  There is no fixed size of the TC.
However, the expected target is between 6 and 12, to ensure adequate
coverage of important areas of expertise, balanced with the ability to
make decisions efficiently.

There is no specific set of requirements or qualifications for TC
membership beyond these rules.

The TC may add contributors to the TC by unanimous consensus.

A TC member may be removed from the TC by voluntary resignation, or by
unanimous consensus of all other TC members.

Changes to TC membership should be posted in the agenda, and may be
suggested as any other agenda item (see "TC Meetings" below).

If an addition or removal is proposed during a meeting, and the full
TC is not in attendance to participate, then the addition or removal
is added to the agenda for the subsequent meeting.  This is to ensure
that all members are given the opportunity to participate in all
membership decisions.  If a TC member is unable to attend a meeting
where a planned membership decision is being made, then their consent
is assumed.

No more than 1/3 of the TC members may be affiliated with the same
employer.  If removal or resignation of a TC member, or a change of
employment by a TC member, creates a situation where more than 1/3 of
the TC membership shares an employer, then the situation must be
immediately remedied by the resignation or removal of one or more TC
members affiliated with the over-represented employer(s).

## TC Meetings

The TC meets weekly on a Google hangout. The meeting is run by a
designated moderator, currently `Mikeal Rogers (@mikeal)`. Each
meeting should be published to Youtube.

Items are added to the TC agenda which are considered contentious or
are modifications of governance, contribution policy, TC membership,
or release process. The intention of the agenda is not to approve or
review all patches, that should happen continuously on GitHub (see
"Contribution Policy").

Any community member or contributor can ask that something be added to
the next meeting's agenda by logging a GitHub Issue. Any TC member or
the moderator can add the item to the agenda by a simple +1. The
moderator and the TC cannot veto or remove items.

Prior to each TC meeting the moderator will email the Agenda to the
TC. TC members can add any items they like to the agenda at the
beginning of each meeting. The moderator and the TC cannot veto or
remove items.

TC may invite persons or representatives from certain projects to
participate in a non-voting capacity. These invitees currently are:

* A representative from [build](https://github.com/node-forward/build)
  chosen by that project.

The moderator is responsible for summarizing the discussion of each
agenda item and send it as a pull request after the meeting.

## Consensus Seeking Process

The TC follows a [Consensus
Seeking](http://en.wikipedia.org/wiki/Consensus-seeking_decision-making)
decision making model.

When an agenda item has appeared to reach a consensus the moderator
will ask "Does anyone object?" as a final call for dissent from the
consensus.

If an agenda item cannot reach a consensus a TC member can call for
either a closing vote or a vote to table the issue to the next
meeting. The call for a vote must be seconded by a majority of the TC
or else the discussion will continue. Simple majority wins.

Note that changes to TC membership require unanimous consensus.  See
"Membership" above.

## Caine's requirements

Hello!

I am pleased to see your valuable contribution to this project. Would you
please mind answering a couple of questions to help me classify this submission
and/or gather required information for the core team members?

### Questions:

* _Issue-only_ Does this issue happen in core, or in some user-space
  module from npm or other source? Please ensure that the test case
  that reproduces this problem is not using any external dependencies.
  If the error is not reproducible with just core modules - it is most
  likely not a io.js problem. _Expected: `yes`_
* Which part of core do you think it might be related to?
  _One of: `debugger, http, assert, buffer, child_process, cluster, crypto,
  dgram, dns, domain, events, fs, http, https, module, net, os, path,
  querystring, readline, repl, smalloc, stream, timers, tls, url, util, vm,
  zlib, c++, docs, other`_ (_label_)
* Which versions of io.js do you think are affected by this?
  _One of: `v0.10, v0.12, v1.0.0`_ (_label_)
* _PR-only_ Does `make test` pass after applying this Pull Request.
  _Expected: `yes`_
* _PR-only_ Is the commit message properly formatted? (See
  CONTRIBUTING.md for more information)
  _Expected: `yes`_

Please provide the answers in an ordered list like this:

1. Answer for the first question
2. Answer for the second question
3. ...

Note that I am just a bot with a limited human-reply parsing abilities,
so please be very careful with numbers and don't skip the questions!

_In case of success I will say:_ `...summoning the core team devs!`.

_In case of validation problem I will say:_ `Sorry, but something is not right
here:`.

Truly yours,
Caine.

### Responsibilities

* indutny: crypto, tls, https, child_process, c++
* trevnorris: buffer, http, https, smalloc
* bnoordhuis: http, cluster, child_process, dgram
