#!/usr/bin/env bash

set -e

rootdir="$(CDPATH= cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
licensefile="${rootdir}/LICENSE"
licensehead="$(sed '/^- /,$d' ${licensefile})"
tmplicense="${rootdir}/~LICENSE.$$"
echo -e "$licensehead" > $tmplicense


# addlicense <library> <location> <license text>
function addlicense {

  echo "
- ${1}, located at ${2}, is licensed as follows:
  \"\"\"
$(echo -e "$3" | sed -e 's/^/    /' -e 's/^    $//' -e 's/ *$//' | sed -e '/./,$!d' | sed -e '/^$/N;/^\n$/D')
  \"\"\"\
" >> $tmplicense

}

# Dependencies bundled in distributions
addlicense "Acorn" "deps/acorn" "$(cat ${rootdir}/deps/acorn/LICENSE)"
addlicense "c-ares" "deps/cares" "$(tail -n +3 ${rootdir}/deps/cares/LICENSE.md)"
addlicense "HTTP Parser" "deps/http_parser" "$(cat deps/http_parser/LICENSE-MIT)"
addlicense "ICU" "deps/icu" "$(cat ${rootdir}/deps/icu/LICENSE)"

addlicense "libuv" "deps/uv" "$(cat ${rootdir}/deps/uv/LICENSE)"
addlicense "OpenSSL" "deps/openssl" \
           "$(sed -e '/^ \*\/$/,$d' -e '/^ [^*].*$/d' -e '/\/\*.*$/d' -e '/^$/d' -e 's/^[/ ]\* *//' ${rootdir}/deps/openssl/openssl/LICENSE)"
addlicense "Punycode.js" "lib/punycode.js" \
           "$(curl -sL https://raw.githubusercontent.com/bestiejs/punycode.js/master/LICENSE-MIT.txt)"
addlicense "V8" "deps/v8" "$(cat ${rootdir}/deps/v8/LICENSE)"
addlicense "zlib" "deps/zlib" \
           "$(sed -e '/The data format used by the zlib library/,$d' -e 's/^\/\* *//' -e 's/^ *//' ${rootdir}/deps/zlib/zlib.h)"

# npm
addlicense "npm" "deps/npm" "$(cat ${rootdir}/deps/npm/LICENSE)"

# Build tools
addlicense "GYP" "tools/gyp" "$(cat ${rootdir}/tools/gyp/LICENSE)"
addlicense "marked" "tools/doc/node_modules/marked" \
           "$(cat ${rootdir}/tools/doc/node_modules/marked/LICENSE)"

# Testing tools
addlicense "cpplint.py" "tools/cpplint.py" \
           "$(sed -e '/^$/,$d' -e 's/^#$//' -e 's/^# //' ${rootdir}/tools/cpplint.py | tail -n +3)"
addlicense "ESLint" "tools/node_modules/eslint" "$(cat ${rootdir}/tools/node_modules/eslint/LICENSE)"
addlicense "babel-eslint" "tools/node_modules/babel-eslint" "$(cat ${rootdir}/tools/node_modules/babel-eslint/LICENSE)"
addlicense "gtest" "deps/gtest" "$(cat ${rootdir}/deps/gtest/LICENSE)"

# nghttp2
addlicense "nghttp2" "deps/nghttp2" "$(cat ${rootdir}/deps/nghttp2/COPYING)"

# remark-cli
addlicense "remark-cli" "tools/remark-cli" "$(cat ${rootdir}/tools/remark-cli/LICENSE)"

# node-inspect
addlicense "node-inspect" "deps/node-inspect" "$(cat ${rootdir}/deps/node-inspect/LICENSE)"

mv $tmplicense $licensefile
