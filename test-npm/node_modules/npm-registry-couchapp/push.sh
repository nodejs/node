#!/bin/bash
export NODE_TLS_REJECT_UNAUTHORIZED=0

c=${npm_package_config_couch}

if [ "$c" == "" ]; then
  cat >&2 <<-ERR
Please set a valid 'npm-registry-couchapp:couch' npm config.

You can put PASSWORD in the setting somewhere to
have it prompt you for a password each time, so
it doesn't get dropped in your config file.

If you have PASSWORD in there, it'll also be read
from the PASSWORD environment variable, so you
can set it in the env and not have to enter it
each time.
ERR
  exit 1
fi

case $c in
  *PASSWORD*)
    if [ "$PASSWORD" == "" ]; then
      echo -n "Password: "
      read -s PASSWORD
    fi
    ;;
  *);;
esac

# echo "couch=$c"

scratch_message () {
  cat <<-EOF

Pushed to scratch ddoc. To make it real, use a COPY request.
Something like this:

curl -u "\$username:\$password" \\
  \$couch/registry/_design/scratch \\
  -X COPY \\
  -H destination:'_design/app?rev=\$rev'

But, before you do that, make sure to fetch the views and give
them time to load, so that real users don't feel the pain of
view generation latency.

You can do it like this using npm:

    npm run load
    # Go get lunch and come back.
    # Go take the dog for a walk, and play some frisbee in the park.
    # If you don't have a dog, go get a puppy, provide it with love
    # and appropriate discipline until it gets older, train it to play
    # frisbee in the park, and then take it to the park and play frisbee.
    # Wait some more.  It's probably almost done now.
    # Make it live:
    npm run copy
EOF
}

c=${c/PASSWORD/$PASSWORD}
c=${c// /%20}

c="$(node -p 'process.argv[1].replace(/\/$/, "")' "$c")"
u="$(node -p 'require("url").resolve(process.argv[1], "_users")' "$c")"

which couchapp

# allow deploy version to be overridden
# with `DEPLOY_VERSION=testing npm start`.
if [ "$DEPLOY_VERSION" == "" ]; then
  DEPLOY_VERSION=`git describe --tags`
fi

DEPLOY_VERSION="$DEPLOY_VERSION" couchapp push registry/app.js "$c" && \
DEPLOY_VERSION="$DEPLOY_VERSION" couchapp push registry/_auth.js "$u" && \
scratch_message && \
exit 0 || \
( ret=$?
  echo "Failed with code $ret"
  exit $ret )
