# Introduction

If you have a patch to the master branch (e.g. an important bug fix) that needs to be merged into one of the production V8 branches, read on.

For the examples, a branched 2.4 version of V8 will be used. Substitute "2.4" with your version number.

**An associated issue is mandatory if a patch is merged. This helps with keeping track of merges.**

# Merge process outlined

The merge process in the Chromium and V8 tracker is driven by labels in the form of
```
Merge-[Status]-[Branch]
```
The currently important labels for V8 are:

  1. Merge-Request-## initiates the process => This fix should be merged into M-##
  1. Merge-Review-## The merge is not approved yet for M-## e.g. because Canary coverage is missing
  1. Merge-Approved-## => Simply means that the Chrome TPM are signing the merge off
  1. Merge-Merged-$BRANCHNUMBER$ => When the merge is done the Merge-Approved label is swapped with this one. $BRANCHNUMBER$ is the name/number of the V8 branch e.g. 4.3 for M-43.

# Instructions for git using the automated script

## How to check if a commit was already merged/reverted

Use mergeinfo.py to get all the commits which are connected to the HASH according to Git.

```
tools/release/mergeinfo.py HASH
```

## Step 1: Run the script

Let's assume you're merging revision af3cf11 to branch 2.4 (please specify full git hashes - abbreviations are used here for simplicity).

```
tools/release/merge_to_branch.py --branch 2.4 af3cf11
```

Run the script with '-h' to display its help message, which includes more options (e.g. you can specify a file containing your patch, or you can reverse a patch, specify a custom commit message, or resume a merging process you've canceled before). Note that the script will use a temporary checkout of v8 - it won't touch your work space.
You can also merge more than one revision at once, just list them all.

```
tools/release/merge_to_branch.py --branch 2.4 af3cf11 cf33f1b sf3cf09
```

## Step 2:  Send a notification letter to hablich@chromium.org

Saying something like this:
```
_Subject:_ Regression fix merged into V8 2.4 branch (Chrome 8)

_Body:_ We have merged a fix to the V8 version 2.4 branch (the version used in Chrome 8)

Version 2.4.9.10: Issue xxx: The parser doesn't parse.
```

# FAQ

## I get an error during merge that is related to tagging. What should I do?
When two people are merging at the same time a race-condition can happen in the merge scripts. If this is the case, contact machenbach@chromium.org and hablich@chromium.org.
## Is there a TL;DR;?
  1. Create issue
  1. Add Merge-Request-{Branch} to the issue
  1. Wait until somebody will add Merge-Approved-{Branch}
  1. Merge