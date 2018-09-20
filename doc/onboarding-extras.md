# Additional Onboarding Information

## Labels

### Subsystems

* `lib/*.js` (`assert`, `buffer`, etc.)
* `build`
* `doc`
* `lib / src`
* `test`
* `tools`

There may be more than one subsystem valid for any particular issue or pull
request.

### General

* `confirmed-bug` - Bugs you have verified exist
* `discuss` - Things that need larger discussion
* `feature request` - Any issue that requests a new feature (usually not PRs)
* `good first issue` - Issues suitable for newcomers to process
* `meta` - For issues whose topic is governance, policies, procedures, etc.

--

* `semver-{minor,major}`
  * be conservative – that is, if a change has the remote *chance* of breaking
    something, go for semver-major
  * when adding a semver label, add a comment explaining why you're adding it
  * minor vs. patch: roughly: "does it add a new method / does it add a new
    section to the docs"
  * major vs. everything else: run last versions tests against this version, if
    they pass, **probably** minor or patch
  * A breaking change helper
    ([full source](https://gist.github.com/chrisdickinson/ba532fa0e4e243fb7b44)):
  ```sh
  SHOW=$(git show-ref -d $(git describe --abbrev=0) | tail -n1 | awk '{print $1}')
  git checkout $(git show -s --pretty='%T' $SHOW) -- test
  make -j4 test
  ```

### LTS/Version labels

We use labels to keep track of which branches a commit should land on:

* `dont-land-on-v?.x`
  * For changes that do not apply to a certain release line
  * Also used when the work of backporting a change outweighs the benefits
* `land-on-v?.x`
  * Used by releasers to mark a PR as scheduled for inclusion in an LTS release
  * Applied to the original PR for clean cherry-picks, to the backport PR
    otherwise
* `backport-requested-v?.x`
  * Used to indicate that a PR needs a manual backport to a branch in order to
    land the changes on that branch
  * Typically applied by a releaser when the PR does not apply cleanly or it
    breaks the tests after applying
  * Will be replaced by either `dont-land-on-v?.x` or `backported-to-v?.x`
* `backported-to-v?.x`
  * Applied to PRs for which a backport PR has been merged
* `lts-watch-v?.x`
  * Applied to PRs which the LTS working group should consider including in a
    LTS release
  * Does not indicate that any specific action will be taken, but can be
    effective as messaging to non-collaborators
* `lts-agenda`
  * For things that need discussion by the LTS working group
  * (for example semver-minor changes that need or should go into an LTS
    release)
* `v?.x`
  * Automatically applied to changes that do not target `master` but rather the
    `v?.x-staging` branch

Once a release line enters maintenance mode, the corresponding labels do not
need to be attached anymore, as only important bugfixes will be included.

### Other Labels

* Operating system labels
  * `macos`, `windows`, `smartos`, `aix`
  * No linux, linux is the implied default
* Architecture labels
  * `arm`, `mips`, `s390`, `ppc`
  * No x86{_64}, since that is the implied default
