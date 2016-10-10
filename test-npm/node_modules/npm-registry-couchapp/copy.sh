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

c=${c/PASSWORD/$PASSWORD}
c=${c// /%20}

c="$(node -p 'process.argv[1].replace(/\/$/, "")' "$c")"
u="$(node -p 'require("url").resolve(process.argv[1], "_users")' "$c")"

if [ "$NO_PROMPT" == "" ]; then
  echo "Did you already run the load-views.sh script? (type 'yes')"
  read didLoad
  if ! [ "$didLoad" == "yes" ]; then
    echo "do that first."
    exit 1
  fi
fi

rev=$(curl -k "$c"/_design/app | json _rev)
if [ "$rev" != "" ]; then
  rev="?rev=$rev"
fi
auth="$(node -pe 'require("url").parse(process.argv[1]).auth || ""' "$c")"
url="$(node -pe 'u=require("url");p=u.parse(process.argv[1]);delete p.auth;u.format(p)' "$c")"
if [ "$auth" != "" ]; then
  auth=(-u "$auth")
fi

curl "$url/_design/scratch" \
  "${auth[@]}" \
  -k \
  -X COPY \
  -H destination:'_design/app'$rev



rev=$(curl -k "$u"/_design/_auth | json _rev)
if [ "$rev" != "" ]; then
  rev="?rev=$rev"
fi
auth="$(node -pe 'require("url").parse(process.argv[1]).auth || ""' "$u")"
url="$(node -pe 'u=require("url");p=u.parse(process.argv[1]);delete p.auth;u.format(p)' "$u")"
if [ "$auth" != "" ]; then
  auth=(-u "$auth")
fi

curl "$url/_design/scratch" \
  "${auth[@]}" \
  -k \
  -X COPY \
  -H destination:'_design/_auth'$rev
