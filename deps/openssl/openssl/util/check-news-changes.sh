#!/bin/bash

# Copyright 2025-2026 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").
# You may not use this file except in compliance with the License.
# You can obtain a copy in the file LICENSE in the source distribution
# or at https://www.openssl.org/source/license.html

#
# This script scans a commit range for common things in prs that we normally
# expect to come with a CHANGE.md or NEWS.md entry.  Its meant to be run prior
# to the release process to aid in the capturing on PR's that got merged without
# a corresponding CHANGES/NEWS entry. Arguments are two tree references to scan between
# looking for PR's that (a) didn't add a NEWS/CHANGES entry and (b) have attributes that
# make them look like they might require one
#

BASE_REF=$(git rev-parse $1)
HEAD_REF=$(git rev-parse $2)

TEMPDIR=$(mktemp -d /tmp/CHECKCHANGES.XXXXXX)

trap "rm -rf $TEMPDIR" EXIT

check_for_news_changes_update() {
    local COMMITS_FILE=$TEMPDIR/$1/commits

    for commit in $(cat $COMMITS_FILE); do
        git show --pretty="format:" --name-only $commit | grep -q "NEWS\.md"
        if [ $? -eq 0 ]; then
            echo "FOUND"
            return
        fi
        git show --pretty="format:" --name-only $commit | grep -q "CHANGES\.md"
        if [ $? -eq 0 ]; then
            echo "FOUND"
            return
       fi
    done
    echo "SCAN"
}

scan_pr_for_news_changes_needs() {
    local COMMITS_FILE=$TEMPDIR/$1/commits
    local pr=$2

    for commit in $(cat $COMMITS_FILE); do
        # Check for the CVE keyword in the commit 
        git show --no-patch --pretty=format:"%B" $commit | grep -q "CVE-"
        if [ $? -eq 0 ]; then
            echo "$pr references a CVE in commit $commit, probably needs a CHANGES.md entry"
            return
        fi

        # Check for public api and config script modifications
        git show --pretty="format:" --name-only $commit | grep -q "include/openssl"
        if [ $? -eq 0 ]; then
            echo "$pr modifies headers in include/openssl in commit $commit, probably needs a CHANGES.md entry"
            return
        fi
        git show --pretty="format:" --name-only $commit | grep -q "Configure"
        if [ $? -eq 0 ]; then
            echo "$pr modifies ./Configure in commit $commit, probably needs a CHANGES.md entry"
            return
        fi
       
    done
}

git log $BASE_REF..$HEAD_REF | grep "Merged from" | sort | uniq | awk '{print $3}' | sed -e"s/)//" > $TEMPDIR/prs

for pr in $(cat $TEMPDIR/prs); do
    PRNUM=$(basename $pr)
    mkdir $TEMPDIR/$PRNUM
    git log --reverse --format=%H --grep="$pr" $BASE_REF..$HEAD_REF > $TEMPDIR/$PRNUM/commits
    FOUND=$(check_for_news_changes_update $PRNUM)
    if [ "$FOUND" == "SCAN" ]; then
        scan_pr_for_news_changes_needs $PRNUM $pr
    fi
done
