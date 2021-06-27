#!/bin/sh

set -e

rootdir="$(CDPATH='' cd "$(dirname "$0")/.." && pwd)"
licensefile="${rootdir}/LICENSE"
licensehead="$(sed '/^- /,$d' "${licensefile}")"
tmplicense="${rootdir}/~LICENSE.$$"
echo "$licensehead" > "$tmplicense"


# addlicense <library> <location> <license text>
addlicense() {

  echo "
- ${1}, located at ${2}, is licensed as follows:
  \"\"\"
$(echo "$3" | sed -e 's/^/    /' -e 's/^    $//' -e 's/ *$//' | sed -e '/./,$!d' | sed -e '/^$/N;/^\n$/D')
  \"\"\"\
" >> "$tmplicense"

}


if ! [ -d "${rootdir}/deps/icu/" ] && ! [ -d "${rootdir}/deps/icu-small/" ]; then
  echo "ICU not installed, run configure to download it, e.g. ./configure --with-intl=small-icu --download=icu"
  exit 1
fi


# Dependencies bundled in distributions
addlicense "Acorn" "deps/acorn" "$(cat "${rootdir}"/deps/acorn/acorn/LICENSE)"
addlicense "c-ares" "deps/cares" "$(tail -n +3 "${rootdir}"/deps/cares/LICENSE.md)"
addlicense "cjs-module-lexer" "deps/cjs-module-lexer" "$(cat "${rootdir}"/deps/cjs-module-lexer/LICENSE)"
if [ -f "${rootdir}/deps/icu/LICENSE" ]; then
  # ICU 57 and following. Drop the BOM
  addlicense "ICU" "deps/icu" \
            "$(sed -e '1s/^[^a-zA-Z ]*ICU/ICU/' -e :a \
              -e 's/<[^>]*>//g;s/	/ /g;s/ +$//;/</N;//ba' "${rootdir}"/deps/icu/LICENSE)"
elif [ -f "${rootdir}/deps/icu/license.html" ]; then
  # ICU 56 and prior
  addlicense "ICU" "deps/icu" \
            "$(sed -e '1,/ICU License - ICU 1\.8\.1 and later/d' -e :a \
              -e 's/<[^>]*>//g;s/	/ /g;s/ +$//;/</N;//ba' "${rootdir}"/deps/icu/license.html)"
elif [ -f "${rootdir}/deps/icu-small/LICENSE" ]; then
  # ICU 57 and following. Drop the BOM
  addlicense "ICU" "deps/icu-small" \
            "$(sed -e '1s/^[^a-zA-Z ]*ICU/ICU/' -e :a \
              -e 's/<[^>]*>//g;s/	/ /g;s/ +$//;/</N;//ba' "${rootdir}"/deps/icu-small/LICENSE)"
elif [ -f "${rootdir}/deps/icu-small/license.html" ]; then
  # ICU 56 and prior
  addlicense "ICU" "deps/icu-small" \
            "$(sed -e '1,/ICU License - ICU 1\.8\.1 and later/d' -e :a \
              -e 's/<[^>]*>//g;s/	/ /g;s/ +$//;/</N;//ba' "${rootdir}"/deps/icu-small/license.html)"
else
  echo "Could not find an ICU license file."
  exit 1
fi

addlicense "libuv" "deps/uv" "$(cat "${rootdir}"/deps/uv/LICENSE)"
addlicense "llhttp" "deps/llhttp" "$(cat deps/llhttp/LICENSE-MIT)"
addlicense "OpenSSL" "deps/openssl" \
           "$(sed -e '/^ \*\/$/,$d' -e '/^ [^*].*$/d' -e '/\/\*.*$/d' -e '/^$/d' -e 's/^[/ ]\* *//' "${rootdir}"/deps/openssl/openssl/LICENSE)"
addlicense "Punycode.js" "lib/punycode.js" \
           "$(curl -sL https://raw.githubusercontent.com/bestiejs/punycode.js/HEAD/LICENSE-MIT.txt)"
addlicense "V8" "deps/v8" "$(cat "${rootdir}"/deps/v8/LICENSE)"
addlicense "SipHash" "deps/v8/src/third_party/siphash" \
           "$(sed -e '/You should have received a copy of the CC0/,$d' -e 's/^\/\* *//' -e 's/^ \* *//' deps/v8/src/third_party/siphash/halfsiphash.cc)"
addlicense "zlib" "deps/zlib" \
           "$(sed -e '/The data format used by the zlib library/,$d' -e 's/^\/\* *//' -e 's/^ *//' "${rootdir}"/deps/zlib/zlib.h)"

# npm
addlicense "npm" "deps/npm" "$(cat "${rootdir}"/deps/npm/LICENSE)"

# Build tools
addlicense "GYP" "tools/gyp" "$(cat "${rootdir}"/tools/gyp/LICENSE)"
addlicense "inspector_protocol" "tools/inspector_protocol" "$(cat "${rootdir}"/tools/inspector_protocol/LICENSE)"
addlicense "jinja2" "tools/inspector_protocol/jinja2" "$(cat "${rootdir}"/tools/inspector_protocol/jinja2/LICENSE)"
addlicense "markupsafe" "tools/inspector_protocol/markupsafe" "$(cat "${rootdir}"/tools/inspector_protocol/markupsafe/LICENSE)"

# Testing tools
addlicense "cpplint.py" "tools/cpplint.py" \
           "$(sed -e '/^$/,$d' -e 's/^#$//' -e 's/^# //' "${rootdir}"/tools/cpplint.py | tail -n +3)"
addlicense "ESLint" "tools/node_modules/eslint" "$(cat "${rootdir}"/tools/node_modules/eslint/LICENSE)"
addlicense "Babel" "tools/node_modules/@babel" "$(cat "${rootdir}"/tools/node_modules/@babel/core/LICENSE)"
addlicense "gtest" "test/cctest/gtest" "$(cat "${rootdir}"/test/cctest/gtest/LICENSE)"

# nghttp2
addlicense "nghttp2" "deps/nghttp2" "$(cat "${rootdir}"/deps/nghttp2/COPYING)"

# large_pages
addlicense "large_pages" "src/large_pages" "$(sed -e '/SPDX-License-Identifier/,$d' -e 's/^\/\///' "${rootdir}"/src/large_pages/node_large_page.h)"

# deep_freeze
addlicense "caja" "lib/internal/freeze_intrinsics.js" "$(sed -e '/SPDX-License-Identifier/,$d' -e 's/^\/\///' "${rootdir}"/lib/internal/freeze_intrinsics.js)"

# brotli
addlicense "brotli" "deps/brotli" "$(cat "${rootdir}"/deps/brotli/LICENSE)"

addlicense "HdrHistogram" "deps/histogram" "$(cat "${rootdir}"/deps/histogram/LICENSE.txt)"

addlicense "highlight.js" "doc/api_assets/highlight.pack.js" \
           "$(curl -sL https://raw.githubusercontent.com/highlightjs/highlight.js/63f367c46f2eeb6f9b7a3545e325eeeb917f9942/LICENSE)"

addlicense "node-heapdump" "src/heap_utils.cc" \
           "$(curl -sL https://raw.githubusercontent.com/bnoordhuis/node-heapdump/0ca52441e46241ffbea56a389e2856ec01c48c97/LICENSE)"

addlicense "rimraf" "lib/internal/fs/rimraf.js" \
           "$(curl -sL https://raw.githubusercontent.com/isaacs/rimraf/0e365ac4e4d64a25aa2a3cc026348f13410210e1/LICENSE)"

addlicense "uvwasi" "deps/uvwasi" "$(cat "${rootdir}"/deps/uvwasi/LICENSE)"
addlicense "ngtcp2" "deps/ngtcp2/ngtcp2/" "$(cat "${rootdir}"/deps/ngtcp2/LICENSE_ngtcp2)"
addlicense "nghttp3" "deps/ngtcp2/nghttp3/" "$(cat "${rootdir}"/deps/ngtcp2/LICENSE_nghttp3)"

mv "$tmplicense" "$licensefile"
