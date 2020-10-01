# @npmcli/git

A utility for spawning git from npm CLI contexts.

This is _not_ an implementation of git itself, it's just a thing that
spawns child processes to tell the system git CLI implementation to do
stuff.

## USAGE

```js
const git = require('@npmcli/git')
git.clone('git://foo/bar.git', 'some-branch', 'some-path', opts) // clone a repo
  .then(() => git.spawn(['checkout', 'some-branch'], {cwd: 'bar'}))
  .then(() => git.spawn(['you get the idea']))
```

## API

Most methods take an options object.  Options are described below.

### `git.spawn(args, opts = {})`

Launch a `git` subprocess with the arguments specified.

All the other functions call this one at some point.

Processes are launched using
[`@npmcli/promise-spawn`](http://npm.im/@npmcli/promise-spawn), with the
`stdioString: true` option enabled by default, since git output is
generally in readable string format.

Return value is a `Promise` that resolves to a result object with `{cmd,
args, code, signal, stdout, stderr}` members, or rejects with an error with
the same fields, passed back from
[`@npmcli/promise-spawn`](http://npm.im/@npmcli/promise-spawn).

### `git.clone(repo, ref = 'HEAD', target = null, opts = {})` -> `Promise<sha String>`

Clone the repository into `target` path (or the default path for the name
of the repository), checking out `ref`.

Return value is the sha of the current HEAD in the locally cloned
repository.

In lieu of a specific `ref`, you may also pass in a `spec` option, which is
a [`npm-package-arg`](http://npm.im/npm-package-arg) object for a `git`
package dependency reference.  In this way, you can select SemVer tags
within a range, or any git committish value.  For example:

```js
const npa = require('npm-package-arg')
git.clone('git@github.com:npm/git.git', '', null, {
  spec: npa('github:npm/git#semver:1.x'),
})

// only gitRange and gitCommittish are relevant, so this works, too
git.clone('git@github.com:npm/git.git', null, null, {
  spec: { gitRange: '1.x' }
})
```

This will automatically do a shallow `--depth=1` clone on any hosts that
are known to support it.  To force a shallow or deep clone, you can set the
`gitShallow` option to `true` or `false` respectively.

### `git.revs(repo, opts = {})` -> `Promise<rev doc Object>`

Fetch a representation of all of the named references in a given
repository.  The resulting doc is intentionally somewhat
[packument](https://www.npmjs.com/package/pacote#packuments)-like, so that
git semver ranges can be applied using the same
[`npm-pick-manifest`](http://npm.im/npm-pick-manifest) logic.

The resulting object looks like:

```js
revs = {
  versions: {
    // all semver-looking tags go in here...
    // version: { sha, ref, rawRef, type }
    '1.0.0': {
      sha: '1bc5fba3353f8e1b56493b266bc459276ab23139',
      ref: 'v1.0.0',
      rawRef: 'refs/tags/v1.0.0',
      type: 'tag',
    },
  },
  'dist-tags': {
    HEAD: '1.0.0',
    latest: '1.0.0',
  },
  refs: {
    // all the advertised refs that can be cloned down remotely
    HEAD: { sha, ref, rawRef, type: 'head' },
    master: { ... },
    'v1.0.0': { ... },
    'refs/tags/v1.0.0': { ... },
  },
  shas: {
    // all named shas referenced above
    // sha: [list, of, refs]
    '6b2501f9183a1753027a9bf89a184b7d3d4602c7': [
      'HEAD',
      'master',
      'refs/heads/master',
    ],
    '1bc5fba3353f8e1b56493b266bc459276ab23139': [ 'v1.0.0', 'refs/tags/v1.0.0' ],
  },
}
```

### `git.is(opts)` -> `Promise<Boolean>`

Resolve to `true` if the path argument refers to the root of a git
repository.

It does this by looking for a file in `${path}/.git/index`, which is not an
airtight indicator, but at least avoids being fooled by an empty directory
or a file named `.git`.

### `git.find(opts)` -> `Promise<String | null>`

Given a path, walk up the file system tree until a git repo working
directory is found.  Since this calls `stat` a bunch of times, it's
probably best to only call it if you're reasonably sure you're likely to be
in a git project somewhere.

Resolves to `null` if not in a git project.

### `git.isClean(opts = {})` -> `Promise<Boolean>`

Return true if in a git dir, and that git dir is free of changes.  This
will resolve `true` if the git working dir is clean, or `false` if not, and
reject if the path is not within a git directory or some other error
occurs.

## OPTIONS

- `retry` An object to configure retry behavior for transient network
  errors with exponential backoff.
  - `retries`: Defaults to `opts.fetchRetries` or 2
  - `factor`: Defaults to `opts.fetchRetryFactor` or 10
  - `maxTimeout`: Defaults to `opts.fetchRetryMaxtimeout` or 60000
  - `minTimeout`: Defaults to `opts.fetchRetryMintimeout` or 1000
- `git` Path to the `git` binary to use.  Will look up the first `git` in
  the `PATH` if not specified.
- `spec` The [`npm-package-arg`](http://npm.im/npm-package-arg) specifier
  object for the thing being fetched (if relevant).
- `fakePlatform` set to a fake value of `process.platform` to use.  (Just
  for testing `win32` behavior on Unix, and vice versa.)
- `cwd` The current working dir for the git command.  Particularly for
  `find` and `is` and `isClean`, it's good to know that this defaults to
  `process.cwd()`, as one might expect.
- Any other options that can be passed to
  [`@npmcli/promise-spawn`](http://npm.im/@npmcli/promise-spawn), or
  `child_process.spawn()`.
