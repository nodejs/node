#!/bin/bash

c=${npm_package_config_couch}

if [ "$c" == "" ]; then
  cat >&2 <<-ERR
Please set a valid 'npmjs.org:couch' npm config.

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

host="$(node -pe 'require("url").parse(process.argv[1]).host' "$c")"
hostname="$(node -pe 'require("url").parse(process.argv[1]).hostname' "$c")"
ips=($(dig +short "$hostname" | egrep '^[0-9]'))

DEPLOY_VERSION=test \
node -pe 'Object.keys(require("./registry/app.js").views).join("\n")' \
| while read view; do
  echo "LOADING: $view"
  curl -Ik "$c/_design/scratch/_view/$view" -H "host:$host"
done
