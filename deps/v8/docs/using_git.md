# Git repository

V8's git repository is located at https://chromium.googlesource.com/v8/v8.git

V8's master branch has also an official git mirror on github: http://github.com/v8/v8-git-mirror.

**Don't just `git-clone` either of these URLs** if you want to build V8 from your checkout, instead follow the instructions below to get everything set up correctly.

## Prerequisites

  1. **Git**. To install using `apt-get`:
```
apt-get install git
```
  1. **depot\_tools**. See [instructions](http://dev.chromium.org/developers/how-tos/install-depot-tools).
  1. For **push access**, you need to setup a .netrc file with your git password:
    1. Go to https://chromium.googlesource.com/new-password - login with your committer account (e.g. @chromium.org account, non-chromium.org ones work too). Note: creating a new password doesn't automatically revoke any previously created passwords.
    1. Follow the instructions in the "Staying Authenticated" section. It would ask you to copy-paste two lines into your ~/.netrc file.
    1. In the end, ~/.netrc should have two lines that look like:
```
machine chromium.googlesource.com login git-yourusername.chromium.org password <generated pwd>
machine chromium-review.googlesource.com login git-yourusername.chromium.org password <generated pwd>
```
    1. Make sure that ~/.netrc file's permissions are 0600 as many programs refuse to read .netrc files which are readable by anyone other than you.


## How to start

Make sure depot\_tools are up-to-date by typing once:

```
gclient
```


Then get V8, including all branches and dependencies:

```
fetch v8
cd v8
```

After that you're intentionally in a detached head state.

Optionally you can specify how new branches should be tracked:

```
git config branch.autosetupmerge always
git config branch.autosetuprebase always
```

Alternatively, you can create new local branches like this (recommended):

```
git new-branch mywork
```

## Staying up-to-date

Update your current branch with git pull. Note that if you're not on a branch, git pull won't work, and you'll need to use git fetch instead.

```
git pull
```

Sometimes dependencies of v8 are updated. You can synchronize those by running

```
gclient sync
```

## Sending code for reviewing

```
git cl upload
```

## Committing

You can use the CQ checkbox on codereview for committing (preferred). See also the [chromium instructions](http://www.chromium.org/developers/testing/commit-queue) for CQ flags and troubleshooting.

If you need more trybots than the default, add the following to your commit message on rietveld (e.g. for adding a nosnap bot):

```
CQ_INCLUDE_TRYBOTS=tryserver.v8:v8_linux_nosnap_rel
```

To land manually, update your branch:

```
git pull --rebase origin
```

Then commit using

```
git cl land
```

# For project members


## Try jobs

### Creating a try job from codereview

  1. Upload a CL to rietveld.
```
git cl upload
```
  1. Try the CL by sending a try job to the try bots like this:
```
git cl try
```
  1. Wait for the try bots to build and you will get an e-mail with the result. You can also check the try state at your patch on codereview.
  1. If applying the patch fails you either need to rebase your patch or specify the v8 revision to sync to:
```
git cl try --revision=1234
```

### Creating a try job from a local branch

  1. Commit some changes to a git branch in the local repo.
  1. Try the change by sending a try job to the try bots like this:
```
git try
```
  1. Wait for the try bots to build and you will get an e-mail with the result. Note: There are issues with some of the slaves at the moment. Sending try jobs from codereview is recommended.

### Useful arguments

The revision argument tells the try bot what revision of the code base will be used for applying your local changes to. Without the revision, our LKGR revision is used as the base (http://v8-status.appspot.com/lkgr).
```
git try --revision=1234
```
To avoid running your try job on all bots, use the --bot flag with a comma-separated list of builder names. Example:
```
git try --bot=v8_mac_rel
```

### Viewing the try server

http://build.chromium.org/p/tryserver.v8/waterfall

### Access credentials

If asked for access credentials, use your @chromium.org email address and your generated password from [googlecode.com](http://code.google.com/hosting/settings).