# Release process

The steps for preparing and making a new release for `temporal_rs` and
its crates are as follows.

1. Fetch the remote refs and tags (`git fetch --tags`)
2. Create and checkout release prep branch
3. Bump the version in the workspace Cargo.toml in the workspace package
   and dependencies sections
4. Update the CHANGELOG with git cliff (See below for more information)
5. Commit changes to branch
   - (Optional / recommended) A dry run publish could be preemptively run with
     `cargo workspaces publish --dry-run`
6. Push to Github/remote, make pull request, and merge release prep.
7. Draft a new release targeting `main` with a `vx.x.x` tag
8. Publish release

## Release post

The release post generally includes the CHANGELOG content; however, an
introduction may be added if the individual making the release would
like to write one.

## Git Cliff

Updating the CHANGELOG with the below commits by running:

```bash
git cliff $PREVIOUS_TAG.. --tag $RELEASE_TAG --prepend CHANGELOG.md
```

`$PREVIOUS_TAG` is the tag for the last release while `$RELEASE_TAG` is
the upcoming release tag.

For example, in order to prep for release `v0.0.9`, the git cliff
command would be:

```bash
git cliff v0.0.8.. --tag v0.0.9 --prepend CHANGELOG.md
```
