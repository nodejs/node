#!/bin/sh

# Shell script to update ESLint in the source tree to the latest release.

# This script must be in the tools directory when it runs because it uses the
# script source file path to determine directories to work in.

cd "$( dirname "$0" )" || exit
rm -rf node_modules/eslint
(
    mkdir eslint-tmp
    cd eslint-tmp || exit

    ROOT="$PWD/../.."
    [ -z "$NODE" ] && NODE="$ROOT/out/Release/node"
    [ -x "$NODE" ] || NODE=`command -v node`
    NPM="$ROOT/deps/npm"

    "$NODE" "$NPM" init --yes

    mkdir eslint-plugin-markdown eslint-plugin-jsdoc
    "$NODE" "$NPM" install --production --no-save --no-package-lock \
        eslint@latest \
        eslint-plugin-markdown@latest \
        eslint-plugin-jsdoc@latest \
        rollup @rollup/plugin-node-resolve @rollup/plugin-commonjs @rollup/plugin-json \
        terser
    node -p '"export default"+JSON.stringify({external:Object.keys(require("eslint/package.json").dependencies),output:{format:"cjs",exports:"default"}})' > rollup.config.mjs

    "$NODE" "$NPM" exec -- rollup -c rollup.config.mjs -p '@rollup/plugin-node-resolve' -p '@rollup/plugin-commonjs' -p '@rollup/plugin-json' -- ./node_modules/eslint-plugin-markdown/index.js | \
    "$NODE" "$NPM" exec -- terser --toplevel --compress --mangle --timings -o ./eslint-plugin-markdown/index.js

    "$NODE" "$NPM" exec -- rollup -c rollup.config.mjs -p '@rollup/plugin-node-resolve' -p '@rollup/plugin-commonjs' -p '@rollup/plugin-json' -- ./node_modules/eslint-plugin-jsdoc/dist/index.js | \
    "$NODE" "$NPM" exec -- terser --toplevel --compress --mangle --timings -o ./eslint-plugin-jsdoc/index.js
    rm -r node_modules

    "$NODE" "$NPM" install --global-style --no-bin-links --production --no-package-lock eslint@latest

    # Use dmn to remove some unneeded files.
    "$NODE" "$NPM" exec -- dmn@2.2.2 -f clean
    # Use removeNPMAbsolutePaths to remove unused data in package.json.
    # This avoids churn as absolute paths can change from one dev to another.
    "$NODE" "$NPM" exec -- removeNPMAbsolutePaths@1.0.4 .
)

mv eslint-tmp/node_modules/eslint node_modules/eslint
mv eslint-tmp/eslint-plugin-markdown node_modules/eslint/node_modules/eslint-plugin-markdown
mv eslint-tmp/eslint-plugin-jsdoc node_modules/eslint/node_modules/eslint-plugin-jsdoc
rm -rf eslint-tmp/
