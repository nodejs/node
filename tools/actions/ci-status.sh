#!/bin/sh

set -xe

JENKINS_JOB_ID=$1
GITHUB_PR_NUMBER=$2

ncu-ci pr "${JENKINS_JOB_ID}" --markdown output.md
gh pr comment "${GITHUB_PR_NUMBER}" --body-file output.md
