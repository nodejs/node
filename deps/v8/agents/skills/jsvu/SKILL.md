---
name: jsvu
description: "How to use jsvu for cross-engine testing."
---

# JSVU (JavaScript engine Version Updater)

Use `jsvu` to install and test across multiple JavaScript engines (e.g., v8, spidermonkey, javascriptcore, chakra).

## Installation and Setup

For non-interactive installation and updating of specific engines on Linux, run:

```bash
npx jsvu --os=linux64 --engines=v8,spidermonkey,javascriptcore
```

You can customize the `--engines` flag with the engines you wish to test (e.g., `v8,v8-debug,spidermonkey,javascriptcore,graaljs`).

## Running Engines

Once installed, the engines are available in `~/.jsvu/bin/`. You can run them directly:

```bash
~/.jsvu/bin/v8 my_test.js
~/.jsvu/bin/spidermonkey my_test.js
~/.jsvu/bin/javascriptcore my_test.js
```

This allows you to easily cross-reference behaviors and bugs against other major JS engines.
