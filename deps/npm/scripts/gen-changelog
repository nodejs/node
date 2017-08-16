#!/bin/sh
# Usage: gen-changelog [comittish]
# Reads all the commits since comittish and produces changelog entries in
# our style as best as it can, appendning them to CHANGELOG.md.  If it
# encounters a git error it won't modify CHANGELOG.md
# @iarna uses this as the first step in producing changelogs for a release.
(node $(npm prefix)/scripts/changelog.js "$@"; cat CHANGELOG.md) > new.md && mv new.md CHANGELOG.md
