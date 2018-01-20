# Pull Requests

There are two fundamental components of the Pull Request process: one concrete
and technical, and one more process oriented. The concrete and technical
component involves the specific details of setting up your local environment
so that you can make the actual changes. This is where we will start.

* [Dependencies](#dependencies)
* [Setting up your local environment](#setting-up-your-local-environment)
  * [Step 1: Fork](#step-1-fork)
  * [Step 2: Branch](#step-2-branch)
* [The Process of Making Changes](#the-process-of-making-changes)
  * [Step 3: Code](#step-3-code)
  * [Step 4: Commit](#step-4-commit)
    * [Commit message guidelines](#commit-message-guidelines)
  * [Step 5: Rebase](#step-5-rebase)
  * [Step 6: Test](#step-6-test)
    * [Test Coverage](#test-coverage)
  * [Step 7: Push](#step-7-push)
  * [Step 8: Opening the Pull Request](#step-8-opening-the-pull-request)
  * [Step 9: Discuss and Update](#step-9-discuss-and-update)
    * [Approval and Request Changes Workflow](#approval-and-request-changes-workflow)
  * [Step 10: Landing](#step-10-landing)
* [Reviewing Pull Requests](#reviewing-pull-requests)
  * [Review a bit at a time](#review-a-bit-at-a-time)
  * [Be aware of the person behind the code](#be-aware-of-the-person-behind-the-code)
  * [Respect the minimum wait time for comments](#respect-the-minimum-wait-time-for-comments)
  * [Abandoned or Stalled Pull Requests](#abandoned-or-stalled-pull-requests)
  * [Approving a change](#approving-a-change)
  * [Accept that there are different opinions about what belongs in Node.js](#accept-that-there-are-different-opinions-about-what-belongs-in-nodejs)
  * [Performance is not everything](#performance-is-not-everything)
  * [Continuous Integration Testing](#continuous-integration-testing)
* [Additional Notes](#additional-notes)
  * [Commit Squashing](#commit-squashing)
  * [Getting Approvals for your Pull Request](#getting-approvals-for-your-pull-request)
  * [CI Testing](#ci-testing)
  * [Waiting Until the Pull Request Gets Landed](#waiting-until-the-pull-request-gets-landed)
  * [Check Out the Collaborator's Guide](#check-out-the-collaborators-guide)

## Dependencies

Node.js has several bundled dependencies in the *deps/* and the *tools/*
directories that are not part of the project proper. Changes to files in those
directories should be sent to their respective projects. Do not send a patch to
Node.js. We cannot accept such patches.

In case of doubt, open an issue in the
[issue tracker](https://github.com/nodejs/node/issues/) or contact one of the
[project Collaborators](https://github.com/nodejs/node/#current-project-team-members).
Node.js has two IRC channels:
[#Node.js](https://webchat.freenode.net/?channels=node.js) for general help and
questions, and
[#Node-dev](https://webchat.freenode.net/?channels=node-dev) for development of
Node.js core specifically.

## Setting up your local environment

To get started, you will need to have `git` installed locally. Depending on
your operating system, there are also a number of other dependencies required.
These are detailed in the [Building guide][].

Once you have `git` and are sure you have all of the necessary dependencies,
it's time to create a fork.

Before getting started, it is recommended to configure `git` so that it knows
who you are:

```text
$ git config --global user.name "J. Random User"
$ git config --global user.email "j.random.user@example.com"
```
Please make sure this local email is also added to your
[GitHub email list](https://github.com/settings/emails) so that your commits
will be properly associated with your account and you will be promoted
to Contributor once your first commit is landed.

### Step 1: Fork

Fork the project [on GitHub](https://github.com/nodejs/node) and clone your fork
locally.

```text
$ git clone git@github.com:username/node.git
$ cd node
$ git remote add upstream https://github.com/nodejs/node.git
$ git fetch upstream
```

### Step 2: Branch

As a best practice to keep your development environment as organized as
possible, create local branches to work within. These should also be created
directly off of the `master` branch.

```text
$ git checkout -b my-branch -t upstream/master
```

## The Process of Making Changes

### Step 3: Code

The vast majority of Pull Requests opened against the `nodejs/node`
repository includes changes to either the C/C++ code contained in the `src`
directory, the JavaScript code contained in the `lib` directory, the
documentation in `docs/api` or tests within the `test` directory.

If you are modifying code, please be sure to run `make lint` from time to
time to ensure that the changes follow the Node.js code style guide.

Any documentation you write (including code comments and API documentation)
should follow the [Style Guide](doc/STYLE_GUIDE.md). Code samples included
in the API docs will also be checked when running `make lint` (or
`vcbuild.bat lint` on Windows).

For contributing C++ code, you may want to look at the
[C++ Style Guide](CPP_STYLE_GUIDE.md).

### Step 4: Commit

It is a recommended best practice to keep your changes as logically grouped
as possible within individual commits. There is no limit to the number of
commits any single Pull Request may have, and many contributors find it easier
to review changes that are split across multiple commits.

```text
$ git add my/changed/files
$ git commit
```

Note that multiple commits often get squashed when they are landed (see the
notes about [commit squashing](#commit-squashing)).

#### Commit message guidelines

A good commit message should describe what changed and why.

1. The first line should:
   - contain a short description of the change (preferably 50 characters or less,
     and no more than 72 characters)
   - be entirely in lowercase with the exception of proper nouns, acronyms, and
   the words that refer to code, like function/variable names
   - be prefixed with the name of the changed subsystem and start with an
   imperative verb. Check the output of `git log --oneline files/you/changed` to
   find out what subsystems your changes touch.

   Examples:
   - `net: add localAddress and localPort to Socket`
   - `src: fix typos in node_lttng_provider.h`


2. Keep the second line blank.
3. Wrap all other lines at 72 columns.

4. If your patch fixes an open issue, you can add a reference to it at the end
of the log. Use the `Fixes:` prefix and the full issue URL. For other references
use `Refs:`.

   Examples:
   - `Fixes: https://github.com/nodejs/node/issues/1337`
   - `Refs: http://eslint.org/docs/rules/space-in-parens.html`
   - `Refs: https://github.com/nodejs/node/pull/3615`

5. If your commit introduces a breaking change (`semver-major`), it should
contain an explanation about the reason of the breaking change, which
situation would trigger the breaking change and what is the exact change.

Breaking changes will be listed in the wiki with the aim to make upgrading
easier.  Please have a look at [Breaking Changes](https://github.com/nodejs/node/wiki/Breaking-changes-between-v4-LTS-and-v6-LTS)
for the level of detail that's suitable.

Sample complete commit message:

```txt
subsystem: explain the commit in one line

Body of commit message is a few lines of text, explaining things
in more detail, possibly giving some background about the issue
being fixed, etc.

The body of the commit message can be several paragraphs, and
please do proper word-wrap and keep columns shorter than about
72 characters or so. That way, `git log` will show things
nicely even when it is indented.

Fixes: https://github.com/nodejs/node/issues/1337
Refs: http://eslint.org/docs/rules/space-in-parens.html
```

If you are new to contributing to Node.js, please try to do your best at
conforming to these guidelines, but do not worry if you get something wrong.
One of the existing contributors will help get things situated and the
contributor landing the Pull Request will ensure that everything follows
the project guidelines.

See [core-validate-commit](https://github.com/evanlucas/core-validate-commit) -
A utility that ensures commits follow the commit formatting guidelines.

### Step 5: Rebase

As a best practice, once you have committed your changes, it is a good idea
to use `git rebase` (not `git merge`) to synchronize your work with the main
repository.

```text
$ git fetch upstream
$ git rebase upstream/master
```

This ensures that your working branch has the latest changes from `nodejs/node`
master.

### Step 6: Test

Bug fixes and features should always come with tests. A
[guide for writing tests in Node.js][] has been
provided to make the process easier. Looking at other tests to see how they
should be structured can also help.

The `test` directory within the `nodejs/node` repository is complex and it is
often not clear where a new test file should go. When in doubt, add new tests
to the `test/parallel/` directory and the right location will be sorted out
later.

Before submitting your changes in a Pull Request, always run the full Node.js
test suite. To run the tests (including code linting) on Unix / macOS:

```text
$ ./configure && make -j4 test
```

And on Windows:

```text
> vcbuild test
```

(See the [Building guide][] for more details.)

Make sure the linter does not report any issues and that all tests pass. Please
do not submit patches that fail either check.

If you want to run the linter without running tests, use
`make lint`/`vcbuild lint`. It will run both JavaScript linting and
C++ linting.

If you are updating tests and just want to run a single test to check it:

```text
$ python tools/test.py -J --mode=release parallel/test-stream2-transform
```

You can execute the entire suite of tests for a given subsystem
by providing the name of a subsystem:

```text
$ python tools/test.py -J --mode=release child-process
```

If you want to check the other options, please refer to the help by using
the `--help` option

```text
$ python tools/test.py --help
```

You can usually run tests directly with node:

```text
$ ./node ./test/parallel/test-stream2-transform.js
```

Remember to recompile with `make -j4` in between test runs if you change code in
the `lib` or `src` directories.

#### Test Coverage

It's good practice to ensure any code you add or change is covered by tests.
You can do so by running the test suite with coverage enabled:

```text
$ ./configure --coverage && make coverage
```

A detailed coverage report will be written to `coverage/index.html` for
JavaScript coverage and to `coverage/cxxcoverage.html` for C++ coverage.

_Note that generating a test coverage report can take several minutes._

To collect coverage for a subset of tests you can set the `CI_JS_SUITES` and
`CI_NATIVE_SUITES` variables:

```text
$ CI_JS_SUITES=child-process CI_NATIVE_SUITES= make coverage
```

The above command executes tests for the `child-process` subsystem and
outputs the resulting coverage report.

Running tests with coverage will create and modify several directories
and files. To clean up afterwards, run:

```text
make coverage-clean
./configure && make -j4.
```

### Step 7: Push

Once you are sure your commits are ready to go, with passing tests and linting,
begin the process of opening a Pull Request by pushing your working branch to
your fork on GitHub.

```text
$ git push origin my-branch
```

### Step 8: Opening the Pull Request

From within GitHub, opening a new Pull Request will present you with a template
that should be filled out:

```markdown
<!--
Thank you for your Pull Request. Please provide a description above and review
the requirements below.

Bug fixes and new features should include tests and possibly benchmarks.

Contributors guide: https://github.com/nodejs/node/blob/master/CONTRIBUTING.md
-->

#### Checklist
<!-- Remove items that do not apply. For completed items, change [ ] to [x]. -->

- [ ] `make -j4 test` (UNIX), or `vcbuild test` (Windows) passes
- [ ] tests and/or benchmarks are included
- [ ] documentation is changed or added
- [ ] commit message follows [commit guidelines](https://github.com/nodejs/node/blob/master/doc/guides/contributing/pull-requests.md#commit-message-guidelines)

#### Affected core subsystem(s)
<!-- Provide affected core subsystem(s) (like doc, cluster, crypto, etc). -->
```

Please try to do your best at filling out the details, but feel free to skip
parts if you're not sure what to put.

Once opened, Pull Requests are usually reviewed within a few days.

### Step 9: Discuss and update

You will probably get feedback or requests for changes to your Pull Request.
This is a big part of the submission process so don't be discouraged! Some
contributors may sign off on the Pull Request right away, others may have
more detailed comments or feedback. This is a necessary part of the process
in order to evaluate whether the changes are correct and necessary.

To make changes to an existing Pull Request, make the changes to your local
branch, add a new commit with those changes, and push those to your fork.
GitHub will automatically update the Pull Request.

```text
$ git add my/changed/files
$ git commit
$ git push origin my-branch
```

It is also frequently necessary to synchronize your Pull Request with other
changes that have landed in `master` by using `git rebase`:

```text
$ git fetch --all
$ git rebase origin/master
$ git push --force-with-lease origin my-branch
```

**Important:** The `git push --force-with-lease` command is one of the few ways
to delete history in `git`. Before you use it, make sure you understand the
risks. If in doubt, you can always ask for guidance in the Pull Request or on
[IRC in the #node-dev channel][].

If you happen to make a mistake in any of your commits, do not worry. You can
amend the last commit (for example if you want to change the commit log).

```text
$ git add any/changed/files
$ git commit --amend
$ git push --force-with-lease origin my-branch
```

There are a number of more advanced mechanisms for managing commits using
`git rebase` that can be used, but are beyond the scope of this guide.

Feel free to post a comment in the Pull Request to ping reviewers if you are
awaiting an answer on something. If you encounter words or acronyms that
seem unfamiliar, refer to this
[glossary](https://sites.google.com/a/chromium.org/dev/glossary).

#### Approval and Request Changes Workflow

All Pull Requests require "sign off" in order to land. Whenever a contributor
reviews a Pull Request they may find specific details that they would like to
see changed or fixed. These may be as simple as fixing a typo, or may involve
substantive changes to the code you have written. In general, such requests
are intended to be helpful, but at times may come across as abrupt or unhelpful,
especially requests to change things that do not include concrete suggestions
on *how* to change them.

Try not to be discouraged. If you feel that a particular review is unfair,
say so, or contact one of the other contributors in the project and seek their
input. Often such comments are the result of the reviewer having only taken a
short amount of time to review and are not ill-intended. Such issues can often
be resolved with a bit of patience. That said, reviewers should be expected to
be helpful in their feedback, and feedback that is simply vague, dismissive and
unhelpful is likely safe to ignore.

### Step 10: Landing

In order to land, a Pull Request needs to be reviewed and [approved][] by
at least one Node.js Collaborator and pass a
[CI (Continuous Integration) test run][]. After that, as long as there are no
objections from other contributors, the Pull Request can be merged. If you find
your Pull Request waiting longer than you expect, see the
[notes about the waiting time](#waiting-until-the-pull-request-gets-landed).

When a collaborator lands your Pull Request, they will post
a comment to the Pull Request page mentioning the commit(s) it
landed as. GitHub often shows the Pull Request as `Closed` at this
point, but don't worry. If you look at the branch you raised your
Pull Request against (probably `master`), you should see a commit with
your name on it. Congratulations and thanks for your contribution!

## Reviewing Pull Requests

All Node.js contributors who choose to review and provide feedback on Pull
Requests have a responsibility to both the project and the individual making the
contribution. Reviews and feedback must be helpful, insightful, and geared
towards improving the contribution as opposed to simply blocking it. If there
are reasons why you feel the PR should not land, explain what those are. Do not
expect to be able to block a Pull Request from advancing simply because you say
"No" without giving an explanation. Be open to having your mind changed. Be open
to working with the contributor to make the Pull Request better.

Reviews that are dismissive or disrespectful of the contributor or any other
reviewers are strictly counter to the [Code of Conduct][].

When reviewing a Pull Request, the primary goals are for the codebase to improve
and for the person submitting the request to succeed. Even if a Pull Request
does not land, the submitters should come away from the experience feeling like
their effort was not wasted or unappreciated. Every Pull Request from a new
contributor is an opportunity to grow the community.

### Review a bit at a time.

Do not overwhelm new contributors.

It is tempting to micro-optimize and make everything about relative performance,
perfect grammar, or exact style matches. Do not succumb to that temptation.

Focus first on the most significant aspects of the change:

1. Does this change make sense for Node.js?
2. Does this change make Node.js better, even if only incrementally?
3. Are there clear bugs or larger scale issues that need attending to?
4. Is the commit message readable and correct? If it contains a breaking change is it clear enough?

When changes are necessary, *request* them, do not *demand* them, and do not
assume that the submitter already knows how to add a test or run a benchmark.

Specific performance optimization techniques, coding styles and conventions
change over time. The first impression you give to a new contributor never does.

Nits (requests for small changes that are not essential) are fine, but try to
avoid stalling the Pull Request. Most nits can typically be fixed by the
Node.js Collaborator landing the Pull Request but they can also be an
opportunity for the contributor to learn a bit more about the project.

It is always good to clearly indicate nits when you comment: e.g.
`Nit: change foo() to bar(). But this is not blocking.`

### Be aware of the person behind the code

Be aware that *how* you communicate requests and reviews in your feedback can
have a significant impact on the success of the Pull Request. Yes, we may land
a particular change that makes Node.js better, but the individual might just
not want to have anything to do with Node.js ever again. The goal is not just
having good code.

### Respect the minimum wait time for comments

There is a minimum waiting time which we try to respect for non-trivial
changes, so that people who may have important input in such a distributed
project are able to respond.

For non-trivial changes, Pull Requests must be left open for *at least* 48
hours during the week, and 72 hours on a weekend. In most cases, when the
PR is relatively small and focused on a narrow set of changes, these periods
provide more than enough time to adequately review. Sometimes changes take far
longer to review, or need more specialized review from subject matter experts.
When in doubt, do not rush.

Trivial changes, typically limited to small formatting changes or fixes to
documentation, may be landed within the minimum 48 hour window.

### Abandoned or Stalled Pull Requests

If a Pull Request appears to be abandoned or stalled, it is polite to first
check with the contributor to see if they intend to continue the work before
checking if they would mind if you took it over (especially if it just has
nits left). When doing so, it is courteous to give the original contributor
credit for the work they started (either by preserving their name and email
address in the commit log, or by using an `Author: ` meta-data tag in the
commit.

### Approving a change

Any Node.js core Collaborator (any GitHub user with commit rights in the
`nodejs/node` repository) is authorized to approve any other contributor's
work. Collaborators are not permitted to approve their own Pull Requests.

Collaborators indicate that they have reviewed and approve of the changes in
a Pull Request either by using GitHub's Approval Workflow, which is preferred,
or by leaving an `LGTM` ("Looks Good To Me") comment.

When explicitly using the "Changes requested" component of the GitHub Approval
Workflow, show empathy. That is, do not be rude or abrupt with your feedback
and offer concrete suggestions for improvement, if possible. If you're not
sure *how* a particular change can be improved, say so.

Most importantly, after leaving such requests, it is courteous to make yourself
available later to check whether your comments have been addressed.

If you see that requested changes have been made, you can clear another
collaborator's `Changes requested` review.

Change requests that are vague, dismissive, or unconstructive may also be
dismissed if requests for greater clarification go unanswered within a
reasonable period of time.

If you do not believe that the Pull Request should land at all, use
`Changes requested` to indicate that you are considering some of your comments
to block the PR from landing. When doing so, explain *why* you believe the
Pull Request should not land along with an explanation of what may be an
acceptable alternative course, if any.

### Accept that there are different opinions about what belongs in Node.js

Opinions on this vary, even among the members of the Technical Steering
Committee.

One general rule of thumb is that if Node.js itself needs it (due to historic
or functional reasons), then it belongs in Node.js. For instance, `url`
parsing is in Node.js because of HTTP protocol support.

Also, functionality that either cannot be implemented outside of core in any
reasonable way, or only with significant pain.

It is not uncommon for contributors to suggest new features they feel would
make Node.js better. These may or may not make sense to add, but as with all
changes, be courteous in how you communicate your stance on these. Comments
that make the contributor feel like they should have "known better" or
ridiculed for even trying run counter to the [Code of Conduct][].

### Performance is not everything

Node.js has always optimized for speed of execution. If a particular change
can be shown to make some part of Node.js faster, it's quite likely to be
accepted. Claims that a particular Pull Request will make things faster will
almost always be met by requests for performance [benchmark results][] that
demonstrate the improvement.

That said, performance is not the only factor to consider. Node.js also
optimizes in favor of not breaking existing code in the ecosystem, and not
changing working functional code just for the sake of changing.

If a particular Pull Request introduces a performance or functional
regression, rather than simply rejecting the Pull Request, take the time to
work *with* the contributor on improving the change. Offer feedback and
advice on what would make the Pull Request acceptable, and do not assume that
the contributor should already know how to do that. Be explicit in your
feedback.

### Continuous Integration Testing

All Pull Requests that contain changes to code must be run through
continuous integration (CI) testing at [https://ci.nodejs.org/][].

Only Node.js core Collaborators with commit rights to the `nodejs/node`
repository may start a CI testing run. The specific details of how to do
this are included in the new Collaborator [Onboarding guide][].

Ideally, the code change will pass ("be green") on all platform configurations
supported by Node.js (there are over 30 platform configurations currently).
This means that all tests pass and there are no linting errors. In reality,
however, it is not uncommon for the CI infrastructure itself to fail on
specific platforms or for so-called "flaky" tests to fail ("be red"). It is
vital to visually inspect the results of all failed ("red") tests to determine
whether the failure was caused by the changes in the Pull Request.

## Additional Notes

### Commit Squashing

When the commits in your Pull Request land, they may be squashed
into one commit per logical change. Metadata will be added to the commit
message (including links to the Pull Request, links to relevant issues,
and the names of the reviewers). The commit history of your Pull Request,
however, will stay intact on the Pull Request page.

For the size of "one logical change",
[0b5191f](https://github.com/nodejs/node/commit/0b5191f15d0f311c804d542b67e2e922d98834f8)
can be a good example. It touches the implementation, the documentation,
and the tests, but is still one logical change. In general, the tests should
always pass when each individual commit lands on the master branch.

### Getting Approvals for Your Pull Request

A Pull Request is approved either by saying LGTM, which stands for
"Looks Good To Me", or by using GitHub's Approve button.
GitHub's Pull Request review feature can be used during the process.
For more information, check out
[the video tutorial](https://www.youtube.com/watch?v=HW0RPaJqm4g)
or [the official documentation](https://help.github.com/articles/reviewing-changes-in-pull-requests/).

After you push new changes to your branch, you need to get
approval for these new changes again, even if GitHub shows "Approved"
because the reviewers have hit the buttons before.

### CI Testing

Every Pull Request needs to be tested
to make sure that it works on the platforms that Node.js
supports. This is done by running the code through the CI system.

Only a Collaborator can start a CI run. Usually one of them will do it
for you as approvals for the Pull Request come in.
If not, you can ask a Collaborator to start a CI run.

### Waiting Until the Pull Request Gets Landed

A Pull Request needs to stay open for at least 48 hours (72 hours on a
weekend) from when it is submitted, even after it gets approved and
passes the CI. This is to make sure that everyone has a chance to
weigh in. If the changes are trivial, collaborators may decide it
doesn't need to wait. A Pull Request may well take longer to be
merged in. All these precautions are important because Node.js is
widely used, so don't be discouraged!

### Check Out the Collaborator's Guide

If you want to know more about the code review and the landing process,
you can take a look at the
[collaborator's guide](https://github.com/nodejs/node/blob/master/COLLABORATOR_GUIDE.md).

[approved]: #getting-approvals-for-your-pull-request
[benchmark results]: ../writing-and-running-benchmarks.md
[Building guide]: ../../../BUILDING.md
[CI (Continuous Integration) test run]: #ci-testing
[Code of Conduct]: https://github.com/nodejs/admin/blob/master/CODE_OF_CONDUCT.md
[guide for writing tests in Node.js]: ../writing-tests.md
[https://ci.nodejs.org/]: https://ci.nodejs.org/
[IRC in the #node-dev channel]: https://webchat.freenode.net?channels=node-dev&uio=d4
[Onboarding guide]: ../onboarding.md
