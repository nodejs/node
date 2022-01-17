# Maintaining npm in Node.js

New pull requests should be opened when a "next" version of npm has
been released. Once the "next" version has been promoted to "latest"
the PR should be updated as necessary.

The specific Node.js release streams the new version will be able to land into
are at the discretion of the release and LTS teams.

This process only covers full updates to new versions of npm. Cherry-picked
changes can be reviewed and landed via the normal consensus seeking process.

## Step 1: Run the update script

In the following examples, `x.y.z` should match the npm version to update to.

```console
$ ./tools/update-npm.sh x.y.z
```

## Step 2: Commit new npm

```console
$ git add -A deps/npm
$ git commit -m "deps: upgrade npm to x.y.z"
```

## Step 3: Update licenses

```console
$ ./configure
$ make -j4
$ ./tools/license-builder.sh
# The following commands are only necessary if there are changes
$ git add .
$ git commit -m "doc: update npm LICENSE using license-builder.sh"
```

Note: please ensure you are only making the updates that are changed by npm.

## Step 4: Apply whitespace fix

```console
$ git rebase --whitespace=fix master
```

## Step 5: Test the build

```console
$ make test-npm
```
