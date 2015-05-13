#!/bin/bash

set -e

# always change the working directory to the io.js/deps directory
cd $(dirname $0)/../deps

# parse depdancy name and version from the first argument
dep=`echo $1 | cut -d'@' -f1`
version=`echo $1 | cut -d'@' -f2`

# bail if we don't have a name or version
if [ -z $dep -o -z $version ]; then
  echo "A dep with version info must be provided. `depname@X.Y.Z`" >&2
  exit 0
fi

# rename libuv to uv
if [ $dep == "libuv" ]; then dep="uv"; fi
# rename http-parser to http_parser
if [ $dep == "http-parser" ]; then dep="http_parser"; fi

# url templates
if [ $dep == "npm" ]; then
  url="https://github.com/npm/npm/archive/v"$version".zip"
elif [ $dep == "uv" ]; then
  url="https://github.com/libuv/libuv/archive/v"$version".zip"
elif [ $dep == "http_parser" ]; then
  url="https://github.com/joyent/http-parser/archive/v"$version".zip"
else
  echo "The dependency "$dep" was not recognized. It should be one of:" >&2
  ls >&2
  exit 0
fi

# make a new tmp dir
rm -rf tmp
mkdir tmp
cd tmp

echo "downloading "$url
# download our depandancy at the specified version
# -O save it to a file
# -L accept redirects (github..)
# -# progress bar
# -f fail with code 22 if the request errors
curl -OL#f $url

echo extracting...
# -q makes unzip completely silent..
unzip -q "v"$version".zip"
rm -f "v"$version".zip"


if [ $dep == "npm" ]; then
  echo building npm "(make release)"...

  # build npm as release (produces .tgz and .zip)
  cd "npm-"$version
  make release

  # cd back tino io.js/deps
  cd ../..

  # remove deps/npm
  rm -rf npm

  # untar the packaged release npm
  tar -zxf "tmp/npm-"$version"/release/npm-"$version".tgz"
else
  echo copying files...

  # FIXME?: assume a extracted single folder
  # github always packs repos like this
  extracted=`ls | cut -d' ' -f1`

  # cd back tino io.js/deps
  cd ..

  # remove deps/$dep
  rm -rf $dep
  mkdir $dep

  # copy everything over
  cp -r "tmp/"$extracted"/" $dep"/"
fi

echo cleaning up...
# remove our tmp directory
rm -rf tmp

# cd into the io.js home directory
cd ..

# get our current git branch for future reference
git_ref=`git symbolic-ref --short -q HEAD`

# just fail if we are in a detached HEAD or something
if [ -z $git_ref ]; then
  echo "Unusable git state. Please checkout a branch." >&2
  exit 0
fi

echo creating a new git branch and commit...
# make a new branche, commit to it, and do automatic fixups.
git checkout -b $dep"@"$version
git add "deps/"$dep
git commit -m "deps: upgrade "$dep" to "$version
git rebase --whitespace=fix $git_ref

echo applying any floating patches...
# initialize stacked git and apply any patches we find
stg init || true
stg repair
stg import --series "deps/floating-patches/patches-"$dep"/series" || true

# cleanup garbage that stgit leaves...
git branch -D $dep"@"$version".stgit"

echo "done updating "$dep"@"$version
