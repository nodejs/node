#!/bin/bash
npmver=$(perl -E "say q{$npm_config_user_agent} =~ m{/(\S+)}")

if semver -r ^3.0.0-0 $npmver > /dev/null; then
	echo "Packaging with $npmver"
else
	echo "[0;41;4mPackaging or installing [1mnpm@$npm_package_version[22m with [1mnpm@$npmver[22m is impossible.[0m" 1>&2
	echo "[0;41;3mPlease install [1mnpm@^3.0.0-0[22m from the registry and use that or run your command with[0m" 1>&2
	echo "[0;41;3mthis version of npm with:[0m" 1>&2
	npmargs=$(node -e "a=$npm_config_argv; console.log(a.original.join(' '))")
	echo "[1m    $npm_node_execpath $PWD $npmargs[0m" 1>&2
	exit 1
fi
