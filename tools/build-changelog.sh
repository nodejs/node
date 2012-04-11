#!/bin/bash
cat ChangeLog \
  | sed -E 's|([^/ ]+/[^#]+)#([0-9]+)|[\1#\2](https://github.com/\1/issues/\2)|g' \
  | sed -E 's| #([0-9]+)| [#\1](https://github.com/joyent/node/issues/\1)|g' \
  | sed -E 's|([0-9]+\.[0-9]+\.[0-9]+),? Version ([0-9]+\.[0-9]+\.[0-9]+)|<a id="v\2"></a>\
# \1 Version \2|g' \
  | sed -E 's|(,? ?)([0-9a-g]{6})[0-9a-g]{34}|\1[\2](https://github.com/joyent/node/commit/\2)|g' \
  | ./node tools/doc/node_modules/.bin/marked > out/doc/changelog-body.html

cat doc/changelog-head.html \
    out/doc/changelog-body.html \
    doc/changelog-foot.html \
  | sed -E 's|__VERSION__|v'$(python tools/getnodeversion.py)'|g' \
  > out/doc/changelog.html

rm out/doc/changelog-body.html
