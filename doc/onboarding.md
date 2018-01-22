# Onboarding

This document is an outline of the things we tell new Collaborators at their
onboarding session.

## One week before the onboarding session

* Confirm that the new Collaborator is using two-factor authentication on their
  GitHub account. Unless two-factor authentication is enabled, do not give an
  account elevated privileges such as the ability to land code in the main
  repository or to start continuous integration (CI) jobs.
* Announce the accepted nomination in a TSC meeting and in the TSC
  mailing list.

## Fifteen minutes before the onboarding session

* Prior to the onboarding session, add the new Collaborator to
  [the Collaborators team](https://github.com/orgs/nodejs/teams/collaborators),
  and to [the Members team](https://github.com/orgs/nodejs/teams/members) if
  they are not already part of it. Note that this is the step that gives the
  account elevated privileges, so do not perform this step (or any subsequent
  steps) unless two-factor authentication is enabled on the new Collaborator's
  GitHub account.

## Onboarding session

* This session will cover:
  * [local setup](#local-setup)
  * [project goals & values](#project-goals--values)
  * [managing the issue tracker](#managing-the-issue-tracker)
  * [reviewing PRs](#reviewing-prs)
  * [landing PRs](#landing-prs)

## Local setup

* git:
  * Make sure you have whitespace=fix: `git config --global --add
    apply.whitespace fix`
  * Always continue to PR from your own github fork
    * Branches in the nodejs/node repository are only for release lines
  * [See "Updating Node.js from Upstream"][]
  * Make a new branch for each PR you submit.
  * Membership: Consider making your membership in the Node.js GitHub
    organization public. This makes it easier to identify Collaborators.
    Instructions on how to do that are available at
    [Publicizing or hiding organization membership][].

* Notifications:
  * Use [https://github.com/notifications](https://github.com/notifications) or
    set up email
  * Watching the main repo will flood your inbox (several hundred notifications
    on typical weekdays), so be prepared

* `#node-dev` on [webchat.freenode.net](https://webchat.freenode.net/) is the
  best place to interact with the TSC / other Collaborators
  * If there are any questions after the session, a good place to ask is there!
  * Presence is not mandatory, but please drop a note there if force-pushing to
    `master`

## Project goals & values

* Collaborators are the collective owners of the project
  * The project has the goals of its contributors

* There are some higher-level goals and values
  * Empathy towards users matters (this is in part why we onboard people)
  * Generally: try to be nice to people!
  * The best outcome is for people who come to our issue tracker to feel like
    they can come back again.

* We have a [Code of Conduct][] that you are expected to follow *and* hold
  others accountable to

## Managing the issue tracker

* You have (mostly) free rein; don't hesitate to close an issue if you are
  confident that it should be closed
  * Be nice about closing issues! Let people know why, and that issues and PRs
    can be reopened if necessary

* [**See "Labels"**](./onboarding-extras.md#labels)
  * There is [a bot](https://github.com/nodejs-github-bot/github-bot) that
    applies subsystem labels (for example, `doc`, `test`, `assert`, or `buffer`)
    so that we know what parts of the code base the pull request modifies. It is
    not perfect, of course. Feel free to apply relevant labels and remove
    irrelevant labels from pull requests and issues.
  * Use the `tsc-review` label if a topic is controversial or isn't coming to a
    conclusion after an extended time.
  * `semver-{minor,major}`:
    * If a change has the remote *chance* of breaking something, use the
      `semver-major` label
    * When adding a semver label, add a comment explaining why you're adding it.
      Do it right away so you don't forget!

* [**See "Who to CC in issues"**](./onboarding-extras.md#who-to-cc-in-issues)
  * This will come more naturally over time
  * For many of the teams listed there, you can ask to be added if you are
    interested
    * Some are WGs with some process around adding people, others are only there
      for notifications

* When a discussion gets heated, you can request that other Collaborators keep
  an eye on it by opening an issue at the private
  [nodejs/moderation](https://github.com/nodejs/moderation) repository.
  * This is a repository to which all members of the `nodejs` GitHub
    organization (not just Collaborators on Node.js core) have access. Its
    contents should not be shared externally.
  * You can find the full moderation policy
    [here](https://github.com/nodejs/TSC/blob/master/Moderation-Policy.md).

## Reviewing PRs

* The primary goal is for the codebase to improve.
* Secondary (but not far off) is for the person submitting code to succeed. A
    pull request from a new contributor is an opportunity to grow the community.
* Review a bit at a time. Do not overwhelm new contributors.
  * It is tempting to micro-optimize and make everything about relative
      performance. Don't succumb to that temptation. We change V8 often.
      Techniques that provide improved performance today may be unnecessary in
      the future.
* Be aware: Your opinion carries a lot of weight!
* Nits (requests for small changes that are not essential) are fine, but try to
  avoid stalling the pull request.
  * Note that they are nits when you comment: `Nit: change foo() to bar().`
  * If they are stalling the pull request, fix them yourself on merge.
* Insofar as possible, issues should be identified by tools rather than human
  reviewers. If you are leaving comments about issues that could be identified
  by tools but are not, consider implementing the necessary tooling.
* Minimum wait for comments time
  * There is a minimum waiting time which we try to respect for non-trivial
      changes, so that people who may have important input in such a distributed
      project are able to respond.
  * For non-trivial changes, leave the pull request open for at least 48 hours
      (72 hours on a weekend).
  * If a pull request is abandoned, check if they'd mind if you took it over
      (especially if it just has nits left).
* Approving a change
  * Collaborators indicate that they have reviewed and approve of the changes in
      a pull request using Github’s approval interface
  * Some people like to comment `LGTM` (“Looks Good To Me”)
  * You have the authority to approve any other collaborator’s work.
  * You cannot approve your own pull requests.
  * When explicitly using `Changes requested`, show empathy – comments will
    usually be addressed even if you don’t use it.
    * If you do, it is nice if you are available later to check whether your
      comments have been addressed
    * If you see that the requested changes have been made, you can clear
      another collaborator's `Changes requested` review.
    * Use `Changes requested` to indicate that you are considering some of your
      comments to block the PR from landing.

* What belongs in Node.js:
  * Opinions vary – it’s good to have a broad collaborator base for that reason!
  * If Node.js itself needs it (due to historic reasons), then it belongs in
    Node.js
    * That is to say, url is there because of http, freelist is there because of
      http, etc.
  * Things that cannot be done outside of core, or only with significant pain
    (for example `async_hooks`)

* Continuous Integration (CI) Testing:
  * [https://ci.nodejs.org/](https://ci.nodejs.org/)
    * It is not automatically run. You need to start it manually.
  * Log in on CI is integrated with GitHub. Try to log in now!
  * You will be using `node-test-pull-request` most of the time. Go there now!
    * Consider bookmarking it: https://ci.nodejs.org/job/node-test-pull-request/
  * To get to the form to start a job, click on `Build with Parameters`. (If you
    don't see it, that probably means you are not logged in!) Click it now!
  * To start CI testing from this screen, you need to fill in two elements on
    the form:
    * The `CERTIFY_SAFE` box should be checked. By checking it, you are
      indicating that you have reviewed the code you are about to test and you
      are confident that it does not contain any malicious code. (We don't want
      people hijacking our CI hosts to attack other hosts on the internet, for
      example!)
    * The `PR_ID` box should be filled in with the number identifying the pull
      request containing the code you wish to test. For example, if the URL for
      the pull request is `https://github.com/nodejs/node/issues/7006`, then put
      `7006` in the `PR_ID`.
    * The remaining elements on the form are typically unchanged with the
      exception of `POST_STATUS_TO_PR`. Check that if you want a CI status
      indicator to be automatically inserted into the PR.
  * If you need help with something CI-related:
    * Use #node-dev (IRC) to talk to other Collaborators.
    * Use #node-build (IRC) to talk to the Build WG members who maintain the CI
      infrastructure.
    * Use the [Build WG repo](https://github.com/nodejs/build) to file issues
      for the Build WG members who maintain the CI infrastructure.

## Landing PRs

* See the Collaborator Guide: [Landing Pull Requests][]

## Exercise: Make a PR adding yourself to the README

* Example:
  [https://github.com/nodejs/node/commit/ce986de829457c39257cd205067602e765768fb0][]
  * For raw commit message: `git log ce986de829457c39257cd205067602e765768fb0
    -1`
* Collaborators are in alphabetical order by GitHub username.
* Optionally, include your personal pronouns.
* Label your pull request with the `doc` subsystem label.
* Run CI on your PR.
* After one or two approvals, land the PR (PRs of this type do not need to wait
  for 48/72 hours to land).
  * Be sure to add the `PR-URL: <full-pr-url>` and appropriate `Reviewed-By:`
    metadata.
  * [`core-validate-commit`][] automates the validation of commit messages.
  * [`node-core-utils`][] automates the generation of metadata and the landing
    process. See the documentation of [`git-node`][].

## Final notes

* Don't worry about making mistakes: everybody makes them, there's a lot to
  internalize and that takes time (and we recognize that!)
* Almost any mistake you could make can be fixed or reverted.
* The existing Collaborators trust you and are grateful for your help!
* Other repositories:
  * [https://github.com/nodejs/TSC](https://github.com/nodejs/TSC)
  * [https://github.com/nodejs/build](https://github.com/nodejs/build)
  * [https://github.com/nodejs/nodejs.org](https://github.com/nodejs/nodejs.org)
  * [https://github.com/nodejs/readable-stream](https://github.com/nodejs/readable-stream)
  * [https://github.com/nodejs/LTS](https://github.com/nodejs/LTS)
  * [https://github.com/nodejs/citgm](https://github.com/nodejs/citgm)
* The Node.js Foundation hosts regular summits for active contributors to the
  Node.js project, where we have face-to-face discussion about our work on the
  project. The foundation has travel funds to cover participants' expenses
  including accommodation, transportation, visa fees etc. if needed. Check out
  the [summit](https://github.com/nodejs/summit) repository for details.

[Code of Conduct]: https://github.com/nodejs/admin/blob/master/CODE_OF_CONDUCT.md
[`core-validate-commit`]: https://github.com/evanlucas/core-validate-commit
[`git-node`]: https://github.com/nodejs/node-core-utils#git-node
[`node-core-utils`]: https://github.com/nodejs/node-core-utils
[Landing Pull Requests]: https://github.com/nodejs/node/blob/master/COLLABORATOR_GUIDE.md#landing-pull-requests
[https://github.com/nodejs/node/commit/ce986de829457c39257cd205067602e765768fb0]: https://github.com/nodejs/node/commit/ce986de829457c39257cd205067602e765768fb0
[Publicizing or hiding organization membership]: https://help.github.com/articles/publicizing-or-hiding-organization-membership/
[See "Updating Node.js from Upstream"]: ./onboarding-extras.md#updating-nodejs-from-upstream
