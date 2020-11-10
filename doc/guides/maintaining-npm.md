# Maintaining npm in Node.js

New pull requests should be opened when a "next" version of npm has
been released. Once the "next" version has been promoted to "latest"
the PR should be updated as necessary.

The specific Node.js release streams the new version will be able to land into
are at the discretion of the release and LTS teams.

This process only covers full updates to new versions of npm. Cherry-picked
changes can be reviewed and landed via the normal consensus seeking process.

## Step 1: Clone npm

```console
$ git clone https://github.com/npm/cli.git npm
$ cd npm
```

or if you already have npm cloned make sure the repo is up to date

```console
$ git remote update -p
$ git reset --hard origin/latest
```

## Step 2: Build release

```console
$ git checkout vX.Y.Z
$ make
$ make release
```

Note: please run `npm dist-tag ls npm` and make sure this is the `latest`
**dist-tag**. `latest` on git is usually released as `next` when it's time to
downstream

## Step 3: Remove old npm

```console
$ cd /path/to/node
$ git remote update -p
$ git checkout -b npm-x.y.z origin/master
$ cd deps
$ rm -rf npm
```

## Step 4: Extract and commit new npm

```console
$ tar zxf /path/to/npm/release/npm-x.y.z.tgz
$ git add -A npm
$ git commit -m "deps: upgrade npm to x.y.z"
$ cd ..
```

## Step 5: Update licenses

```console
$ ./configure
$ make -j4
$ ./tools/license-builder.sh
# The following commands are only necessary if there are changes
$ git add .
$ git commit -m "doc: update npm LICENSE using license-builder.sh"
```

Note: please ensure you are only making the updates that are changed by npm.

## Step 6: Apply Whitespace fix

```console
$ git rebase --whitespace=fix master
```

## Step 7: Test the build

```console
$ make test-npm
```
