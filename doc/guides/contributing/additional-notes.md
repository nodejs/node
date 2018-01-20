# Additional Notes

* [Commit Squashing](#commit-squashing)
* [Getting Approvals for your Pull Request](#getting-approvals-for-your-pull-request)
* [CI Testing](#ci-testing)
* [Waiting Until the Pull Request Gets Landed](#waiting-until-the-pull-request-gets-landed)
* [Check Out the Collaborator's Guide](#check-out-the-collaborators-guide)
* [Helpful Resources](#helpful-resources)

## Commit Squashing

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

## Getting Approvals for Your Pull Request

A Pull Request is approved either by saying LGTM, which stands for
"Looks Good To Me", or by using GitHub's Approve button.
GitHub's Pull Request review feature can be used during the process.
For more information, check out
[the video tutorial](https://www.youtube.com/watch?v=HW0RPaJqm4g)
or [the official documentation](https://help.github.com/articles/reviewing-changes-in-pull-requests/).

After you push new changes to your branch, you need to get
approval for these new changes again, even if GitHub shows "Approved"
because the reviewers have hit the buttons before.

## CI Testing

Every Pull Request needs to be tested
to make sure that it works on the platforms that Node.js
supports. This is done by running the code through the CI system.

Only a Collaborator can start a CI run. Usually one of them will do it
for you as approvals for the Pull Request come in.
If not, you can ask a Collaborator to start a CI run.

## Waiting Until the Pull Request Gets Landed

A Pull Request needs to stay open for at least 48 hours (72 hours on a
weekend) from when it is submitted, even after it gets approved and
passes the CI. This is to make sure that everyone has a chance to
weigh in. If the changes are trivial, collaborators may decide it
doesn't need to wait. A Pull Request may well take longer to be
merged in. All these precautions are important because Node.js is
widely used, so don't be discouraged!

## Check Out the Collaborator's Guide

If you want to know more about the code review and the landing process,
you can take a look at the
[collaborator's guide](https://github.com/nodejs/node/blob/master/COLLABORATOR_GUIDE.md).

## Helpful Resources

The following additional resources may be of assistance:

* [How to create a Minimal, Complete, and Verifiable example](https://stackoverflow.com/help/mcve)
* [core-validate-commit](https://github.com/evanlucas/core-validate-commit) -
  A utility that ensures commits follow the commit formatting guidelines.
