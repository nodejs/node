#!/bin/bash

set -e

export MLSDK="${MLSDK:-/mnt/c/Users/avaer/MagicLeap/mlsdk/v0.19.0}"

git checkout base-11.6.0
./configure
make -j4
cp out/Release/torque .

git checkout 11.6.0

rm -Rf out
../magicleap-js/hack-toolchain.js
./android-configure
make -j4

read -r -d '' s <<eof
process.argv.slice(1).forEach(p => {
  fs.writeFileSync(p,
    fs.readFileSync(p, 'utf8')
      .replace(/\r\n/gm, '\n')
      .replace(/\r/gm, '\n')
      .replace(/([\\\\\/][_\-\.a-zA-Z0-9]+)\n(:)/gm, (all, a, b) => a + b)
      .replace(/(\.?[a-zA-Z]+):/g, (all, a) => /^[a-zA-Z]$/.test(a) ? ('/mnt/' + a.toLowerCase() + '/') : all)
      .replace(/\/+/gm, '/')
  );
})
eof
find 'out/Release/.deps//mnt/c/Users/avaer/Documents/GitHub/node-magicleap/out/Release/obj.target' -type f -name '*.o.d' -exec node -e "$s" "{}" +;
# f=ares_getopt.o.d
# find out -type f -name '*.d' -exec mac2unix "{}" +;
# find out -type f -name '*.d' -exec sed -i 's/\([^ ]\)\\/\1\//g' "{}" +;
# find out -type f -name '*.d' -exec sed -i 's/\(^\|[^\.]\|[^\.][^\.]\)c:/\1\/mnt\/c\//g' "{}" +;
# find out -type f -name "$f" -exec sed -i ':a;N;$!ba;s/\\\n:\n/\n/g' "{}" +;
# find out -type f -name '*.d' -exec sed -i ':a;N;$!ba;s/\(\.[a-z]+\)\n:/\1:/g' "{}" +;
# git checkout src/*.d
make -j4

rm -f libnode.a
"$MLSDK/tools/toolchains/bin/aarch64-linux-android-ar" -M <libnode.mri