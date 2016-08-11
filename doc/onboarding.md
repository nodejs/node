# Onboarding

This document is an outline of the things we tell new Collaborators at their
onboarding session.

* Prior to the onboarding session, add the new Collaborators to
[the Collaborators team](https://github.com/orgs/nodejs/teams/collaborators).

## **thank you** for doing this

  * going to cover four things:
    * local setup
    * some project goals & values
    * issues, labels, and reviewing code
    * merging code

## setup

  * notifications setup
    * use https://github.com/notifications or set up email
    * watching the main repo will flood your inbox, so be prepared


  * git:
    * make sure you have whitespace=fix: `git config --global --add core.whitespace fix`
    * usually PR from your own github fork
    * [**See "Updating Node.js from Upstream"**](./onboarding-extras.md#updating-nodejs-from-upstream)
    * make new branches for all commits you make!


  * `#node-dev` on `chat.freenode.net` is the best place to interact with the CTC / other collaborators


## a little deeper about the project

  * collaborators are effectively part owners
    * the project has the goals of its contributors


  * but, there are some higher-level goals and values
    * not everything belongs in core (if it can be done reasonably in userland, let it stay in userland)
    * empathy towards users matters (this is in part why we onboard people)
    * generally: try to be nice to people


## managing the issue tracker

  * you have (mostly) free rein – don't hesitate to close an issue if you are confident that it should be closed
    * this will come more naturally over time
    * IMPORTANT: be nice about closing issues, let people know why, and that issues and PRs can be reopened if necessary
    * Still need to follow the Code of Conduct.


  * Labels:
    * There is [a bot](https://github.com/nodejs-github-bot/github-bot) that applies subsystem labels (for example, `doc`, `test`, `assert`, or `buffer`) so that we know what parts of the code base the pull request modifies. It is not perfect, of course. Feel free to apply relevant labels and remove irrelevant labels from pull requests and issues.
    * [**See "Labels"**](./onboarding-extras.md#labels)
    * Use the `ctc-agenda` if a topic is controversial or isn't coming to a conclusion after an extended time.
    * `semver-{minor,major}`:
      * If a change has the remote *chance* of breaking something, use `semver-major`
      * When adding a semver label, add a comment explaining why you're adding it. Do it right away so you don't forget!

  * Notifying humans
    * [**See "Who to CC in issues"**](./onboarding-extras.md#who-to-cc-in-issues)
    * will also come more naturally over time


  * reviewing:
    * primary goal is for the codebase to improve
    * secondary (but not far off) is for the person submitting code to succeed
      * helps grow the community
      * and draws new people into the project
    * Review a bit at a time. It is **very important** to not overwhelm newer people.
      * it is tempting to micro-optimize / make everything about relative perf,
        don't succumb to that temptation. we change v8 a lot more often now, contortions
        that are zippy today may be unnecessary in the future
    * be aware: your opinion carries a lot of weight!
    * nits are fine, but try to avoid stalling the PR
      * note that they are nits when you comment
      * if they really are stalling nits, fix them yourself on merge (but try to let PR authors know they can fix these)
      * improvement doesn't have to come all at once
    * minimum wait for comments time
      * There is a minimum waiting time which we try to respect for non-trivial changes, so that people who may have important input in such a distributed project are able to respond.
        * It may help to set time limits and expectations:
          * the collaborators are very distributed so it is unlikely that they will be looking at stuff the same time as you are.
          * before merging code: give folks at least one working day to respond: "If no one objects, tomorrow at <time> I'll merge this in."
            * please always either specify your timezone, or use UTC time
            * set reminders
          * check in on the code every once in a while (set reminders!)
      * 48 hours for non-trivial changes, and 72 hours on weekends.
      * if a PR is abandoned, check if they'd mind if you took it over (especially if it just has nits left)
      * you have the power to `LGTM` another collaborator or TSC / CTC members' work


  * what belongs in node:
    * opinions vary, but I find the following helpful:
    * if node itself needs it (due to historic reasons), then it belongs in node
      * that is to say, url is there because of http, freelist is there because of http, et al
    * also, things that cannot be done outside of core, or only with significant pain (example: async-wrap)


  * Continuous Integration (CI) Testing:
    * https://ci.nodejs.org/
    * It is not automatically run. You need to start it manually.
    * Log in on CI is integrated with GitHub. Try to log in now!
    * You will be using `node-test-pull-request` most of the time. Go there now!
    * To get to the form to start a job, click on `Build with Parameters`. (If you don't see it, that probably means you are not logged in!) Click it now!
    * To start CI testing from this screen, you need to fill in two elements on the form:
      * The `CERTIFY_SAFE` box should be checked. By checking it, you are indicating that you have reviewed the code you are about to test and you are confident that it does not contain any malicious code. (We don't want people hijacking our CI hosts to attack other hosts on the internet, for example!)
      * The `PR_ID` box should be filled in with the number identifying the pull request containing the code you wish to test. For example, if the URL for the pull request is https://github.com/nodejs/node/issues/7006, then put `7006` in the `PR_ID`.
      * The remaining elements on the form are typically unchanged with the exception of `POST_STATUS_TO_PR`. Check that if you want a CI status indicator to be automatically inserted into the PR.


## process for getting code in

  * the collaborator guide is a great resource: https://github.com/nodejs/node/blob/master/COLLABORATOR_GUIDE.md#technical-howto


  * no one (including TSC or CTC members) pushes directly to master without review
    * an exception is made for release commits only


  * one "LGTM" is usually sufficient, except for semver-major changes
    * the more the better
    * semver-major (breaking) changes must be reviewed in some form by the CTC


  * be sure to wait before merging non-trivial changes
    * 48 hours for non-trivial changes, and 72 hours on weekends.


  * **make sure to run the PR through CI before merging!** (Except for documentation PRs)


  * once code is ready to go in:
    * [**See "Landing PRs"**](#landing-prs) below


  * what if something goes wrong?
    * ping a CTC member
    * `#node-dev` on freenode
    * force-pushing to fix things after is allowed for ~10 minutes, be sure to notify people in IRC if you need to do this, but avoid it
    * Info on PRs that don't like to apply found under [**"If `git am` fails"**](./onboarding-extras.md#if-git-am-fails).


## Landing PRs

* Please never use GitHub's green "Merge Pull Request" button.
  * If you do, please force-push removing the merge.

Update your `master` branch (or whichever branch you are landing on, almost always `master`)

* [**See "Updating Node.js from Upstream"**](./onboarding-extras.md#updating-nodejs-from-upstream)

Landing a PR

* if it all looks good, `curl -L 'url-of-pr.patch' | git am`
* `git rebase -i upstream/master`
* squash into logical commits if necessary
* `./configure && make -j8 test` (`-j8` builds node in parallel with 8 threads. adjust to the number of cores (or processor-level threads) your processor has (or slightly more) for best results.)
* Amend the commit description
  * commits should follow `subsystem[,subsystem]: small description\n\nbig description\n\n<metadata>`
  * first line 50 columns, all others 72
  * add metadata:
    * `Fixes: <full-issue-url>`
    * `Reviewed-By: human <email>`
      * Easiest to use `git log` then do a search
      * (`/Name` + `enter` (+ `n` as much as you need to) in vim)
      * Only include collaborators who have commented "LGTM"
    * `PR-URL: <full-pr-url>`
* `git push upstream master`
    * close the original PR with "Landed in `<commit hash>`".


## exercise: make PRs adding yourselves to the README

  * Example: https://github.com/nodejs/node/commit/7b09aade8468e1c930f36b9c81e6ac2ed5bc8732
    * to see full URL: `git log 7b09aade8468e1c930f36b9c81e6ac2ed5bc8732 -1`
  * Collaborators in alphabetical order by username
  * Label your pull request with the `doc` subsystem label
  * If you would like to run CI on your PR, feel free to
  * Make sure to added the `PR-URL: <full-pr-url>`!


## final notes

  * don't worry about making mistakes: everybody makes them, there's a lot to internalize and that takes time (and we recognize that!)
  * very few (no?) mistakes are unrecoverable
  * the existing node committers trust you and are grateful for your help!
  * other repos:
    * https://github.com/nodejs/dev-policy
    * https://github.com/nodejs/NG
    * https://github.com/nodejs/api
    * https://github.com/nodejs/build
    * https://github.com/nodejs/docs
    * https://github.com/nodejs/nodejs.org
    * https://github.com/nodejs/readable-stream
    * https://github.com/nodejs/LTS
