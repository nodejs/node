## pre-setup

Ensure everyone is added to https://github.com/orgs/nodejs/teams/collaborators


## onboarding to nodejs

### intros


### **thank you** for doing this

  * going to cover four things:
    * local setup
    * some project goals & values
    * issues, labels, and reviewing code
    * merging code


### setup:

  * notifications setup
    * use https://github.com/notifications or set up email
    * watching the main repo will flood your inbox, so be prepared


  * git:
    * make sure you have whitespace=fix: `git config --global --add core.whitespace fix`
    * usually PR from your own github fork
    * [**See "Updating Node.js from Upstream"**](./onboarding-extras.md#updating-nodejs-from-upstream)
    * make new branches for all commits you make!


  * `#node-dev` on `chat.freenode.net` is the best place to interact with the CTC / other collaborators


### a little deeper about the project

  * collaborators are effectively part owners
    * the project has the goals of its contributors


  * but, there are some higher-level goals and values
    * not everything belongs in core (if it can be done reasonably in userland, let it stay in userland)
    * empathy towards users matters (this is in part why we onboard people)
    * generally: try to be nice to people


### managing the issue tracker

  * you have (mostly) free rein – don't hesitate to close an issue if you are confident that it should be closed
    * this will come more naturally over time
    * IMPORTANT: be nice about closing issues, let people know why, and that issues and PRs can be reopened if necessary
    * Still need to follow the Code of Conduct.


  * labels:
    * generally sort issues by a concept of "subsystem" so that we know what part(s) of the codebase it touches, though there are also other useful labels.
     * [**See "Labels"**](./onboarding-extras.md#labels)
    * `ctc-agenda` if a topic is controversial or isn't coming to a conclusion after an extended time.
    * `semver-{minor,major}`:
      * be conservative – that is, if a change has the remote *chance* of breaking something, go for `semver-major`
      * when adding a semver label, add a comment explaining why you're adding it
        * it's cached locally in your brain at that moment!


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


  * CI testing:
    * lives here: https://ci.nodejs.org/
      * not automatically run - some of the platforms we test do not have full sandboxing support so we need to ensure what we run on it isn't potentially malicious
    * make sure to log in – we use github authentication so it should be seamless
    * go to "node-test-pull-request" and "Build with parameters"
    * fill in the pull request number without the `#`, and check the verification that you have reviewed the code for potential malice
      * The other options shouldn't need to be adjusted in most cases.
    * link to the CI run in the PR by commenting "CI: <ci run link>"


### process for getting code in:

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


### Landing PRs

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
    * `PR-URL: <full-pr-url>`
* `git push upstream master`
    * close the original PR with "Landed in `<commit hash>`".


### exercise: make PRs adding yourselves to the README.

  * Example: https://github.com/nodejs/node/commit/7b09aade8468e1c930f36b9c81e6ac2ed5bc8732
    * to see full URL: `git log 7b09aade8468e1c930f36b9c81e6ac2ed5bc8732 -1`
  * Collaborators in alphabetical order by username
  * Label your pull request with the `doc` subsystem label
  * If you would like to run CI on your PR, feel free to
  * Make sure to added the `PR-URL: <full-pr-url>`!


### final notes:

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
