#!/bin/sh

# <a class="link-man" href="zip_errors.html">
LINKBASE='http://pubs.opengroup.org/onlinepubs/9699919799/functions/'

sed -E -e 's,(<a class="Xr" href=")([^"]*)(">),\1'"$LINKBASE"'\2\3,g' -e "s,$LINKBASE"'(libzip|zip),\1,g'
