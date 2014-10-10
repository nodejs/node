# CONTRIBUTING

The node.js project welcomes new contributors.  This document will guide you
through the process.


### FORK

Fork the project [on GitHub](https://github.com/node-forward/node) and check out
your copy.

```sh
$ git clone git@github.com:username/node.git
$ cd node
$ git remote add upstream git://github.com/node-forward/node.git
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


[stability index page]: https://github.com/node-forward/node/blob/master/doc/api/documentation.markdown
[issue tracker]: https://github.com/node-forward/node/issues
[node.js mailing list]: http://groups.google.com/group/nodejs
[IRC]: http://webchat.freenode.net/?channels=node.js

### COMMITTER GUIDE

Committers who are merging their work and the work of others have a few other
rules to follow.

  - Always include the `Reviewed-by: You Name <your-email>` in the commit
  message.
  - In commit messages also include `Fixes:` that either includes the
  **full url** (e.g. `https://github.com/node-forward/node/issues/...`), and/or
  the hash and commit message if the commit fixes a bug in a previous commit.
  - PR's should include their full `PR-URL:` so it's easy to trace a commit
  back to the conversation that lead up to that change.
  - Double check PR's to make sure the persons **full name** and email
  address are correct before merging.
  - Except when updating dependencies, all commits should be self contained.
  Meaning, every commit should pass all tests. Makes it much easier when
  bisecting to find a breaking change.

# Governance

This repository (node-forward/node) is jointly governed by a technical
committee, commonly referred to as the "TC."

Initial membership invitations to the TC were given to individuals who had
been active contributors to Node. Current membership is:

```
Fedor Indutny (@indutny)
Trevor Norris (@trevnorris)
Ben Noordhuis (@bnoordhuis)
Isaac Z. Schlueter (@isaacs)
Nathan Rajlich (@TooTallNate)
Bert Belder (@piscisaureus)
```

Invitations were also given to `TJ Fontaine (@tjfontaine)` and
`Alexis Campailla (@orangemocha)` who have not accepted but are
still invited to participate without accepting a role or
officially endorsing this effort.

Additionally the TC may invite persons or representatives from certain projects
to participate in a non-voting capacity. These invitees currently are:

* A representative from [build](https://github.com/node-forward/build) chosen
by that project.

The TC has final authority over this project including:

* Project governance and process
* Contribution policy
* GitHub repository hosting

The TC can change its governance model if they deem it necessary. The current
governance rules are:

* [Consensus Seeking](http://en.wikipedia.org/wiki/Consensus-seeking_decision-making)
* Motions with voting when consensus cannot be reached.
* Quorum of 2/3 (66%), simple definite majority wins.
* No more than 1/3 (34%) of the TC membership can be affiliated with the same
employer.

## TC Meetings

The TC meets weekly on a Google hangout. The meeting is run by a designated
moderator, currently `Mikeal Rogers (@mikeal)`. Each meeting should be
published to Youtube.

## Contributor Policy

Individuals making significant and valuable contributions are given
commit-access to the project. These individuals are identified by the TC and
discussed during the weekly TC meeting.

If you make a significant contribution and are not considered for commit-access
log an issue and it will be brought up in the next TC meeting.

Internal pull-requests to solicit feedback are required for any other
non-trivial contribution but left to the discretion of the contributor.

For significant changes wait a full 48 hours (72 hours if it spans a weekend)
before merging so that active contributors who are distributed throughout the
world have a chance to weigh in.

Controversial changes and **very** significant changes should not be merged
until they have been discussed by the TC which will make any final decisions.

TC members nominate contributors to be added to the TC which the TC will vote
on. They can nominate any individual during any meeting.

## Developer's Certificate of Origin 1.0

By making a contribution to this project, I certify that:

* (a) The contribution was created in whole or in part by me and I have the
right to submit it under the open source license indicated in the file; or
* (b) The contribution is based upon previous work that, to the best of my
knowledge, is covered under an appropriate open source license and I have the
right under that license to submit that work with modifications, whether
created in whole or in part by me, under the same open source license (unless
I am permitted to submit under a different license), as indicated in the
file; or
* (c) The contribution was provided directly to me by some other person who
certified (a), (b) or (c) and I have not modified it.

## Code of Conduct

This Code of Conduct is adapted from [Rust's wonderful CoC](https://github.com/rust-lang/rust/wiki/Note-development-policy#conduct).

* We are committed to providing a friendly, safe and welcoming environment for
all, regardless of gender, sexual orientation, disability, ethnicity, religion,
or similar personal characteristic.
* Please avoid using overtly sexual nicknames or other nicknames that might
detract from a friendly, safe and welcoming environment for all.
* Please be kind and courteous. There's no need to be mean or rude.
* Respect that people have differences of opinion and that every design or
implementation choice carries a trade-off and numerous costs. There is seldom
a right answer.
* Please keep unstructured critique to a minimum. If you have solid ideas you
want to experiment with, make a fork and see how it works.
* We will exclude you from interaction if you insult, demean or harass anyone.
That is not welcome behaviour. We interpret the term "harassment" as including
the definition in the [Citizen Code of Conduct](http://citizencodeofconduct.org/);
if you have any lack of clarity about what might be included in that concept,
please read their definition. In particular, we don't tolerate behavior that
excludes people in socially marginalized groups.
* Private harassment is also unacceptable. No matter who you are, if you feel
you have been or are being harassed or made uncomfortable by a community
member, please contact one of the channel ops or any of the TC members
immediately with a capture (log, photo, email) of the harassment if possible.
Whether you're a regular contributor or a newcomer, we care about making this
community a safe place for you and we've got your back.
* Likewise any spamming, trolling, flaming, baiting or other attention-stealing
behaviour is not welcome.
* Avoid the use of personal pronouns in code comments or documentation. There
is no need to address persons when explaining code (e.g. "When the developer")
