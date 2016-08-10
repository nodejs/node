V8 Inspector support for Node.js
================================

This repository is an intermediate repository that gathers the dependencies for
Node.js support for the [Chrome Debug Protocol][https://developer.chrome.com/devtools/docs/debugger-protocol].

* third_party/v8_inspector/platform/v8_inspector: vendored from https://chromium.googlesource.com/chromium/src/third_party/WebKit/Source/platform/v8_inspector
* third_party/v8_inspector/platform/inspector_protocol: vendored from https://chromium.googlesource.com/chromium/src/third_party/WebKit/Source/platform/inspector_protocol
* third_party/jinja2: vendored from https://github.com/mitsuhiko/jinja2
  * The `tests/` directory `ext/jinja.el` file have been deleted.
* third_party/markupsafe: vendored from https://github.com/mitsuhiko/markupsafe
