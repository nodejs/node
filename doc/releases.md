Node.js Release Process
=====================

This document describes the technical aspects of the node.js release process. The intended audience is those who have been authorized by the Node.js Foundation Technical Steering Committee (TSC) to create, promote and sign official release builds for node.js, hosted on <https://nodejs.org>.

## Who can make a release?

Release authorization is given by the node.js TSC. Once authorized, an individual must be have the following:

### 1. Jenkins Release Access

There are three relevant Jenkins jobs that should be used for a release flow:

**a.** **Test runs:** **[node-test-pull-request](https://ci.nodejs.org/job/node-test-pull-request/)** is used for a final full-test run to ensure that the current *HEAD* is stable.

**b.** **Nightly builds:** (optional) **[iojs+release](https://ci.nodejs.org/job/iojs+release/)** can be used to create a nightly release for the current *HEAD* if public test releases are required. Builds triggered with this job are published straight to <https://nodejs.org/download/nightly/> and are available for public download.

**c.** **Release builds:** **[iojs+release](https://ci.nodejs.org/job/iojs+release/)** does all of the work to build all required release assets. Promotion of the release files is a manual step once they are ready (see below).

The [Node.js build team](https://github.com/nodejs/build) is able to provide this access to individuals authorized by the TC.

### 2. <nodejs.org> Access

The _dist_ user on nodejs.org controls the assets available in <https://nodejs.org/download/> (note that <https://nodejs.org/dist/> is an alias for <https://nodejs.org/download/release/>).

The Jenkins release build slaves upload their artifacts to the web server as the _staging_ user, the _dist_ user has access to move these assets to public access (the _staging_ user does not, for security purposes).

Nightly builds are promoted automatically on the server by a cron task for the _dist_ user.

Release builds require manual promotion by an individual with SSH access to the server as the _dist_ user. The [Node.js build team](https://github.com/nodejs/build) is able to provide this access to individuals authorized by the TC.

### 3. A Publicly Listed GPG Key

A SHASUMS256.txt file is produced for every promoted build, nightly and releases. Additionally for releases, this file is signed by the individual responsible for that release. In order to be able to verify downloaded binaries, the public should be able to check that the SHASUMS256.txt file has been signed by someone who has been authorized to create a release.

The GPG keys should be fetchable from a known third-party keyserver, currently the SKS Keyservers at <https://sks-keyservers.net> are recommended. Use the [submission](https://sks-keyservers.net/i/#submit) form to submit a new GPG key. Keys should be fetchable via:

```
gpg --keyserver pool.sks-keyservers.net --recv-keys <FINGERPRINT>
```

The key you use may be a child/subkey of an existing key.

Additionally, full GPG key fingerprints for individuals authorized to release should be listed in the Node.js GitHub README.md file.

## How to create a release

Notes:

 - Dates listed below as _"YYYY-MM-DD"_ should be the date of the release **as UTC**. Use `date -u +'%Y-%m-%d'` to find out what this is.
 - Version strings are listed below as _"vx.y.z"_, substitute for the release version.

### 1. Ensure that HEAD Is Stable

Run a **[node-test-pull-request](https://ci.nodejs.org/job/node-test-pull-request/)** test run to ensure that the build is stable and the HEAD commit is ready for release.

### 2. Produce a Nightly Build _(optional)_

If there is a reason to produce a test release for the purpose of having others try out installers or specifics of builds, produce a nightly build using **[iojs+release](https://ci.nodejs.org/job/iojs+release/)** and wait for it to drop in <https://nodejs.org/download/nightly/>. Follow the directions and enter a proper length commit sha, a date string and select "nightly" for "disttype".

This is particularly recommended if there has been recent work relating to the OS X or Windows installers as they are not tested in any way by CI.

### 3. Update the _CHANGELOG.md_

Collect a formatted list of commits since the last release. Use [changelog-maker](https://github.com/rvagg/changelog-maker) (available from npm: `npm install changelog-maker -g`) to do this.

```
$ changelog-maker --group
```

Note that changelog-maker counts commits since the last tag and if the last tag in the repository was not on the current branch you may have to supply a `--start-ref` argument:

```
$ changelog-maker --group --start-ref v2.3.1
```

The _CHANGELOG.md_ entry should take the following form:

```
## YYYY-MM-DD, Version x.y.z, @releaser

### Notable changes

* List interesting changes here
* Particularly changes that are responsible for minor or major version bumps
* Also be sure to look at any changes introduced by dependencies such as npm
* ... and include any notable items from there

### Known issues

See https://github.com/nodejs/node/labels/confirmed-bug for complete and current list of known issues.

* Include this section if there are any known problems with this release
* Scan GitHub for unresolved problems that users may need to be aware of

### Commits

* Include the full list of commits since the last release here
```

### 4. Update _src/node_version.h_

The following macros should already be set for the release since they will have been updated directly following the last release. They shouldn't require changing:

```
#define NODE_MAJOR_VERSION x
#define NODE_MINOR_VERSION y
#define NODE_PATCH_VERSION z
```

However, the `NODE_VERSION_IS_RELEASE` macro needs to be set to `1` for the build to be produced with a version string that does not have a trailing pre-release tag:

```
#define NODE_VERSION_IS_RELEASE 1
```

**Also consider whether to bump `NODE_MODULE_VERSION`**:

This macro is used to signal an ABI version for native addons. It currently has two common uses in the community:

* Determining what API to work against for compiling native addons, e.g. [NAN](https://github.com/rvagg/nan) uses it to form a compatibility-layer for much of what it wraps.
* Determining the ABI for downloading pre-built binaries of native addons, e.g. [node-pre-gyp](https://github.com/mapbox/node-pre-gyp) uses this value as exposed via `process.versions.modules` to help determine the appropriate binary to download at install-time.

The general rule is to bump this version when there are _breaking ABI_ changes and also if there are non-trivial API changes. The rules are not yet strictly defined, so if in doubt, please confer with someone that will have a more informed perspective, such as a member of the NAN team.

**Note** that it is current TSC policy to bump major version when ABI changes. If you see a need to bump `NODE_MODULE_VERSION` then you should consult the TSC, commits may need to be reverted or a major version bump may need to happen.

### 5. Create Release Commit

The _CHANGELOG.md_ and _src/node_version.h_ changes should be the final commit that will be tagged for the release.

When committing these to git, use the following message format:

```
YYYY-MM-DD node.js vx.y.z Release

Notable changes:

* Copy the notable changes list here, reformatted for plain-text
```

### 6. Push to GitHub

Note that it is not essential that the release builds be created from the Node.js repository, they may be created from your own fork if you desire. It is preferable, but not essential that the commits remain the same between that used to build and the tagged commit in the Node.js repository.

### 7. Produce Release Builds

Use **[iojs+release](https://ci.nodejs.org/job/iojs+release/)** to produce release artifacts. Enter the commit that you want to build from and select "release" for "disttype".

Artifacts from each slave are uploaded to Jenkins and are available if further testing is required. Use this opportunity particularly to test OS X and Windows installers if there are any concerns. Click through to the individual slaves for a run to find the artifacts.

All release slaves should achieve "SUCCESS" (and be green, not red). A release with failures should not be promoted, there are likely problems to be investigated.

You can rebuild the release as many times as you need prior to promoting them if you encounter problems.

Note that you do not have to wait for the ARM builds if they are take longer than the others. It is only necessary to have the main Linux (x64 and x86), OS X .pkg and .tar.gz, Windows (x64 and x86) .msi and .exe, source, headers and docs (both produced currently by an OS X slave). i.e. the slaves with "arm" in their name don't need to have finished to progress to the next step. However, **if you promote builds _before_ ARM builds have finished, you must repeat the promotion step for the ARM builds when they are ready**.

### 8. Tag and Sign the Release Commit

Tag the release as <b><code>vx.y.z</code></b> and sign **using the same GPG key that will be used to sign SHASUMS256.txt**.

```
$ git tag <vx.y.z> <commit-sha> -sm 'YYYY-MM-DD node.js vz.y.x Release'
```

Push the tag to GitHub.

```
$ git push origin <vx.y.z>
```

### 9. Set Up For the Next Release

Edit _src/node_version.h_ again and:

* Increment `NODE_PATCH_VERSION` by one
* Change `NODE_VERSION_IS_RELEASE` back to `0`

Commit this change with:

```
$ git commit -am 'Working on vx.y.z' # where 'z' is the incremented patch number
```

This sets up the branch so that nightly builds are produced with the next version number _and_ a pre-release tag.

### 10. Promote and Sign the Release Builds

**It is important that the same individual who signed the release tag be the one to promote the builds as the SHASUMS256.txt file needs to be signed with the same GPG key!**

When you are confident that the build slaves have properly produced usable artifacts and uploaded them to the web server you can promote them to release status. This is done by interacting with the web server via the _dist_ user.

The _tools/release.sh_ script should be used to promote and sign the build. When run, it will perform the following actions:

**a.** Select a GPG key from your private keys, it will use a command similar to: `gpg --list-secret-keys` to list your keys. If you don't have any keys, it will bail (why are you releasing? Your tag should be signed!). If you have only one key, it will use that. If you have more than one key it will ask you to select one from the list. Be sure to use the same key that you signed your git tag with.

**b.** Log in to the server via SSH and check for releases that can be promoted, along with the list of artifacts. It will use the `dist-promotable` command on the server to find these. You will be asked, for each promotable release, whether you want to proceed. If there is more than one release to promote (there shouldn't be), be sure to only promote the release you are responsible for.

**c.** Log in to the server via SSH and run the promote script for the given release. The command on the server will be similar to: `dist-promote vx.y.z`. After this step, the release artifacts will be available for download and a SHASUMS256.txt file will be present. The release will still be unsigned, however.

**d.** Download SHASUMS256.txt to your computer using SCP into a temporary directory.

**e.** Sign the SHASUMS256.txt file using a command similar to: `gpg --default-key YOURKEY --clearsign /path/to/SHASUMS256.txt`. You will be prompted by GPG for your password for this to work. The signed file will be named SHASUMS256.txt.asc.

**f.** Output an ASCII armored version of your public GPG key, using a command similar to: `gpg --default-key YOURKEY --armor --export --output /path/to/SHASUMS256.txt.gpg`. This does not require your password and is mainly a convenience for users although not the recommended way to get a copy of your key.

**g.** Upload the SHASUMS256.txt\* files back to the server into the release directory.

If you didn't wait for ARM builds in the previous step before promoting the release, you should re-run _tools/release.sh_ after the ARM builds have finished and it will move the ARM artifacts into the correct location and you will be prompted to re-sign SHASUMS256.txt.

### 11. Check the Release

Your release should be available at <https://nodejs.org/dist/vx.y.z/> and also <https://nodejs.org/dist/latest/>. Check that the appropriate files are in place, you may also want to check that the binaries are working as appropriate and have the right internal version strings. Check that the API docs are available at <https://nodejs.org/api/>. Check that the release catalog files are correct at <https://nodejs.org/dist/index.tab> and <https://nodejs.org/dist/index.json>.

### 12. Announce

The nodejs.org website will automatically rebuild and include the new version. You simply need to announce the build, preferably via twitter with a message such as:

> v2.3.2 of @official_iojs is out @ https://nodejs.org/dist/latest/ changelog @ https://github.com/nodejs/node/blob/master/CHANGELOG.md#2015-07-01-version-232-rvagg … something here about notable changes

### 13. Celebrate

_In whatever form you do this..._
