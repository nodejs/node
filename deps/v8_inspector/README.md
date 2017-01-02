V8 Inspector support for Node.js
================================

This repository is an intermediate repository that gathers the dependencies for
Node.js support for the [Chrome Debug Protocol][https://developer.chrome.com/devtools/docs/debugger-protocol].

* include: vendored from v8/include
* src/inspector: vendored from v8/src/inspector
* third_party/WebKit/platform/inspector_protocol: vendored from https://chromium.googlesource.com/chromium/src/third_party/WebKit/Source/platform/inspector_protocol
* third_party/jinja2: vendored from https://github.com/mitsuhiko/jinja2
  * The `tests/` directory `ext/jinja.el` file have been deleted.
* third_party/markupsafe: vendored from https://github.com/mitsuhiko/markupsafe
