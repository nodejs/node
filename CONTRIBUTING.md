# Contributing to Node.js

## Code of Conduct

Please read the
[Code of Conduct](https://github.com/nodejs/TSC/blob/master/CODE_OF_CONDUCT.md)
which explains the minimum behavior expectations for Node.js contributors.

## Issue Contributions

When opening issues or commenting on existing issues, please make sure
discussions are related to concrete technical issues with Node.js.

* For general help using Node.js, please file an issue at the
[Node.js help repository](https://github.com/nodejs/help/issues).

* Discussion of non-technical topics (such as intellectual property and
trademark) should use the
[Technical Steering Committee (TSC) repository](https://github.com/nodejs/TSC/issues).

## Code Contributions

This section will guide you through the contribution process.

### Step 1: Fork

Fork the project [on GitHub](https://github.com/nodejs/node) and clone your fork
locally.

```text
$ git clone git@github.com:username/node.git
$ cd node
$ git remote add upstream https://github.com/nodejs/node.git
```

#### Which branch?

For developing new features and bug fixes, the `master` branch should be pulled
and built upon.

#### Dependencies

Node.js has several bundled dependencies in the *deps/* and the *tools/*
directories that are not part of the project proper. Changes to files in those
directories should be sent to their respective projects. Do not send a patch to
Node.js. We cannot accept such patches.

In case of doubt, open an issue in the
[issue tracker](https://github.com/nodejs/node/issues/) or contact one of the
[project Collaborators](https://github.com/nodejs/node/#current-project-team-members).
Node.js has two IRC channels:
[#Node.js](http://webchat.freenode.net/?channels=node.js) for general help and
questions, and
[#Node-dev](http://webchat.freenode.net/?channels=node-dev) for development of
Node.js core specifically.

### Step 2: Branch

Create a branch and start hacking:

```text
$ git checkout -b my-branch -t origin/master
```

Any text you write should follow the [Style Guide](doc/STYLE_GUIDE.md),
including comments and API documentation.

### Step 3: Commit

Make sure git knows your name and email address:

```text
$ git config --global user.name "J. Random User"
$ git config --global user.email "j.random.user@example.com"
```

Add and commit:

```text
$ git add my/changed/files
$ git commit
```

### Commit message guidelines

The commit message should describe what changed and why.

1. The first line should:
   - contain a short description of the change
   - be 50 characters or less
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

### Step 4: Rebase

Use `git rebase` (not `git merge`) to synchronize your work with the main
repository.

```text
$ git fetch upstream
$ git rebase upstream/master
```

### Step 5: Test

Bug fixes and features should come with tests. Read the
[guide for writing tests in Node.js](./doc/guides/writing-tests.md). Looking at
other tests to see how they should be structured can also help. Add your
tests in the `test/parallel/` directory if you are unsure where to put them.

To run the tests (including code linting) on Unix / macOS:

```text
$ ./configure && make -j4 test
```

Windows:

```text
> vcbuild test
```

(See the [BUILDING.md](./BUILDING.md) for more details.)

Make sure the linter does not report any issues and that all tests pass. Please
do not submit patches that fail either check.

If you want to run the linter without running tests, use
`make lint`/`vcbuild lint`. It will run both JavaScript linting and
C++ linting.

If you are updating tests and just want to run a single test to check it:

```text
$ python tools/test.py -J --mode=release parallel/test-stream2-transform
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

### Step 6: Push

```text
$ git push origin my-branch
```

Pull requests are usually reviewed within a few days.

### Step 7: Discuss and update

You will probably get feedback or requests for changes to your Pull Request.
This is a big part of the submission process so don't be discouraged!

To make changes to an existing Pull Request, make the changes to your branch.
When you push that branch to your fork, GitHub will automatically update the
Pull Request.

You can push more commits to your branch:

```text
$ git add my/changed/files
$ git commit
$ git push origin my-branch
```

Or you can rebase against master:

```text
$ git fetch --all
$ git rebase origin/master
$ git push --force-with-lease origin my-branch
```

Or you can amend the last commit (for example if you want to change the commit
log).

```text
$ git add any/changed/files
$ git commit --amend
$ git push --force-with-lease origin my-branch
```

**Important:** The `git push --force-with-lease` command is one of the few ways
to delete history in git. Before you use it, make sure you understand the risks.
If in doubt, you can always ask for guidance in the Pull Request or on
[IRC in the #node-dev channel](https://webchat.freenode.net?channels=node-dev&uio=d4).

Feel free to post a comment in the Pull Request to ping reviewers if you are
awaiting an answer on something. If you encounter words or acronyms that
seem unfamiliar, refer to this
[glossary](https://sites.google.com/a/chromium.org/dev/glossary).

Note that multiple commits often get squashed when they are landed (see the
notes about [commit squashing](#commit-squashing)).

### Step 8: Landing

In order to land, a Pull Request needs to be reviewed and
[approved](#getting-approvals-for-your-pull-request) by
at least one Node.js Collaborator and pass a
[CI (Continuous Integration) test run](#ci-testing).
After that, as long as there are no objections
from a Collaborator, the Pull Request can be merged. If you find your
Pull Request waiting longer than you expect, see the
[notes about the waiting time](#waiting-until-the-pull-request-gets-landed).

When a collaborator lands your Pull Request, they will post
a comment to the Pull Request page mentioning the commit(s) it
landed as. GitHub often shows the Pull Request as `Closed` at this
point, but don't worry. If you look at the branch you raised your
Pull Request against (probably `master`), you should see a commit with
your name on it. Congratulations and thanks for your contribution!

## Additional Notes

### Commit Squashing

When the commits in your Pull Request land, they will be squashed
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
