#!/bin/bash

#set -e

test_node_versions="0.8.28 0.10.40 0.12.7"
test_iojs_versions="1.8.4 2.4.0 3.3.0"

# borrows from https://github.com/rvagg/dnt/

# Simple setup function for a container:
#  setup_container(image id, base image, commands to run to set up)
setup_container() {
  local ID=$1
  local BASE=$2
  local RUN=$3

  # Does this image exist? If yes, ignore
  docker inspect "$ID" &> /dev/null
  if [[ $? -eq 0 ]]; then
    echo "Found existing container [$ID]"
  else
    # No such image, so make it
    echo "Did not find container [$ID], creating..."
    docker run -i $BASE /bin/bash -c "$RUN"
    sleep 2
    docker commit $(docker ps -l -q) $ID
  fi
}

# Run tests inside each of the versioned containers, copy cwd into npm's copy of node-gyp
# so it'll be invoked by npm when a compile is needed
#  run_tests(version, test-commands)
run_tests() {
  local VERSION=$1
  local RUN=$2

  RUN="rsync -aAXx --delete --exclude .git --exclude build /node-gyp-src/ /usr/lib/node_modules/npm/node_modules/node-gyp/;
    /bin/su -s /bin/bash node-gyp -c 'cd && ${RUN}'"

  docker run --rm -v ~/.npm/:/dnt/.npm/ -v $(pwd):/node-gyp-src/:ro -i node-gyp-test/${VERSION} /bin/bash -c "$RUN"
}


# A base image with build tools and a user account
setup_container "node-gyp-test/base" "ubuntu:14.04" "
  apt-get update &&
  apt-get install -y build-essential python git rsync curl &&
  adduser --gecos node-gyp --home /node-gyp/ --disabled-login node-gyp &&
  echo "node-gyp:node-gyp" | chpasswd
"

# An image on top of the base containing clones of repos we want to use for testing
setup_container "node-gyp-test/clones" "node-gyp-test/base" "
  cd /node-gyp/ && git clone https://github.com/justmoon/node-bignum.git &&
  cd /node-gyp/ && git clone https://github.com/bnoordhuis/node-buffertools.git &&
  chown -R node-gyp.node-gyp /node-gyp/
"

# An image for each of the node versions we want to test with that version installed and the latest npm
for v in $test_node_versions; do
  setup_container "node-gyp-test/${v}" "node-gyp-test/clones" "
    curl -sL https://nodejs.org/dist/v${v}/node-v${v}-linux-x64.tar.gz | tar -zxv --strip-components=1 -C /usr/ &&
    npm install npm@latest -g &&
    node -v && npm -v
  "
done

# An image for each of the io.js versions we want to test with that version installed and the latest npm
for v in $test_iojs_versions; do
  setup_container "node-gyp-test/${v}" "node-gyp-test/clones" "
    curl -sL https://iojs.org/dist/v${v}/iojs-v${v}-linux-x64.tar.gz | tar -zxv --strip-components=1 -C /usr/ &&
    npm install npm@latest -g &&
    node -v && npm -v
  "
done


# Run the tests for all of the test images we've created,
# we should see node-gyp doing its download, configure and run thing
# _NOTE: bignum doesn't compile on 0.8 currently so it'll fail for that version only_
for v in $test_node_versions $test_iojs_versions; do
  run_tests $v "
    cd node-buffertools && npm install --loglevel=info && npm test && cd &&
    cd node-bignum && npm install --loglevel=info && npm test
  "
done