#!/bin/bash

## This is to be used once jenkins has finished building the release

set -e

if [[ ! -e ../node-website/Makefile ]];
then
  echo "node-website must be checked out one level up"
  exit 1
fi

stability="$(python tools/getstability.py)"
NODE_STABC="$(tr '[:lower:]' '[:upper:]' <<< ${stability:0:1})${stability:1}"
NODE_STABL="$stability"
GITHUB_USERNAME=

function usage
{
  echo "usage: sh tools/node-release-post-build.sh -u gh_username"
  exit 1
}

while [ "$1" != "" ]; do
  case $1 in
    -u | --github-username ) shift
                             GITHUB_USERNAME=$1
                             ;;
    * )                      usage
                             exit 1
  esac
  shift
done

if [ "$GITHUB_USERNAME" = "" ];
then
  usage
fi

echo "Building for $stability"

scp staging@nodejs.org:archive/node/tmp/v$(python tools/getnodeversion.py)/SHASUM* .
FILES="SHASUMS SHASUMS256"
for i in $FILES ; do gpg -s $i.txt; gpg --clearsign $i.txt; done
scp SHASUM* staging@nodejs.org:archive/node/tmp/v$(python tools/getnodeversion.py)/
 
ssh nodejs.org mkdir -p "dist/v$(python tools/getnodeversion.py)/{x64,docs}"
ssh nodejs.org ln -s ../dist/v$(python tools/getnodeversion.py)/docs docs/v$(python tools/getnodeversion.py)

ssh root@nodejs.org mv /home/staging/archive/node/tmp/v$(python tools/getnodeversion.py)/* /home/node/dist/v$(python tools/getnodeversion.py)/
ssh root@nodejs.org chown -R node:other /home/node/dist/v$(python tools/getnodeversion.py)

# tag the release
# should be the same key used to sign the shasums
git tag -sm "$(bash tools/changelog-head.sh)" v$(python tools/getnodeversion.py)
 
# push to github
git push git@github.com:$GITHUB_USERNAME/node v$(python tools/getnodeversion.py)-release --tags

# blog post and email
make email.md
( echo ""
  echo "Shasums:"
  echo '```'
  cat SHASUMS.txt.asc
  echo '```' ) >> email.md
( echo -n "date: "
  date
  echo -n "version: "
  python tools/getnodeversion.py
  echo "category: release"
  echo "title: Node v"$(python tools/getnodeversion.py)" ($NODE_STABC)"
  echo "slug: node-v"$(python tools/getnodeversion.py | sed 's|\.|-|g')"-$NODE_STABL"
  echo ""
  cat email.md ) > ../node-website/doc/blog/release/v$(python tools/getnodeversion.py).md

if [ "$stability" = "unstable" ];
then
  BRANCH="master"
else
  ## This needs to happen here because the website depends on the current node
  ## node version.
  if [ "$stability" = "stable" ]
  then
    echo $(python tools/getnodeversion.py) > ../node-website/STABLE
  fi

  BRANCH="v$(python tools/getnodeversion.py | sed -E 's#\.[0-9]+$##')"
fi

echo "Merging back into $BRANCH"

# merge back into mainline stable branch
git checkout $BRANCH
git merge --no-ff v$(python tools/getnodeversion.py)-release
 
# change the version number, set isrelease = 0
## TODO automagic.
vim src/node_version.h
git commit -am "Now working on "$(python tools/getnodeversion.py)

git push git@github.com:$GITHUB_USERNAME/node $BRANCH

echo "Now go do the website stuff"
