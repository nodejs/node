# How to Backport a Pull Request to a Release Line

## Staging Branches

Each release line has a staging branch that serves as a workspace for preparing releases.
The branch format is `vN.x-staging`, where `N` is the major release number.

For active staging branches, refer to the [Release Schedule][].

## Identifying Backport Needs

If a cherry-pick from `main` doesn't merge cleanly with a staging branch, the pull request
will be labeled for the release line (e.g., `backport-requested-vN.x`). This indicates that
manual backporting is required.

## Criteria for Backporting

The "Current" release line is more flexible than LTS lines. LTS branches, detailed in the [Release Plan][],
require commits to mature in the Current release for at least two weeks before backporting.

## Labeling Backport Issues and PRs

Use the following labels, with `N` in `vN.x` denoting the major release number:

| Label                   | Description                                                         |
| ----------------------- | ------------------------------------------------------------------- |
| backport-blocked-vN.x   | PRs for `vN.x-staging` blocked by pending backports from other PRs. |
| backport-open-vN.x      | Indicates an open backport for the PR.                              |
| backport-requested-vN.x | PR awaiting manual backport to `vN.x-staging`.                      |
| backported-to-vN.x      | PR successfully backported to `vN.x-staging`.                       |
| baking-for-lts          | PRs awaiting LTS release after maturation in Current.               |
| lts-watch-vN.x          | PRs possibly included in `vN.x` LTS releases.                       |
| vN.x                    | Issues or PRs impacting `vN.x-staging` (or reproducible on vN.x).   |

## Submitting a Backport Pull Request

Follow these steps to backport a PR (e.g., #123) to the `v20.x` release line:

### Automated Process

1. Ensure [`@node-core/utils`][] is installed.

2. Execute [`git node backport`][] command:

   ```bash
   # Example: Backport PR 123 to v20.x-staging
   git node backport 123 --to=20
   ```

3. Proceed to step 5 in the Manual section below.

### Manual Process

1. Check out the `v20.x-staging` branch.

2. Update the local staging branch from the remote.

3. Create a new branch based on `v20.x-staging`:

   ```bash
   git fetch upstream v20.x-staging:v20.x-staging -f
   git checkout -b backport-123-to-v20.x v20.x-staging
   ```

4. Resolve conflicts during cherry-pick:

   ```console
   git cherry-pick $SHA # Use commit hash
   ```

5. Resolve conflicts using `git add` and `git cherry-pick --continue`.

6. Keep the commit message unchanged or modify as needed.

7. Verify with `make -j4 test`.

8. Push changes to your fork.

9. Open a pull request:

   * Target `v20.x-staging`.
   * Title format: `[v20.x backport] <commit title>` (e.g., `[v20.x backport] process: improve performance of nextTick`).
   * Allow maintainer access.
   * Reference the original PR in the description.
   * Add `Backport-PR-URL:` metadata and re-push.

10. Run [`node-test-pull-request`][] CI job (with default `REBASE_ONTO`).

11. Replace `backport-requested-v20.x` with `backport-open-v20.x` on the original PR.

12. Resolve conflicts with `git pull --rebase upstream v20.x-staging` if needed.

Once merged, update the original PR's label from `backport-open-v20.x` to `backported-to-v20.x`.

[Release Plan]: https://github.com/nodejs/Release#release-plan
[Release Schedule]: https://github.com/nodejs/Release#release-schedule
[`@node-core/utils`]: https://github.com/nodejs/node-core-utils
[`git node backport`]: https://github.com/nodejs/node-core-utils/blob/main/docs/git-node.md#git-node-backport
[`node-test-pull-request`]: https://ci.nodejs.org/job/node-test-pull-request/build
