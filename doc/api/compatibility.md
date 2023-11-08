# Compatibility

Other runtimes exist, both proprietary and open-source, that implement some or
all of the Node.js API. To clarify expectations for users of these runtimes, Node.js
defines three levels of compatibility for alternative runtimes. A runtime can
be said to be "Node.js compatible" at a given level, and must target a specific
version of Node.js (which can change from release to release of the runtime).

Node.js does not provide any certification of these compatibility levels.
Alternative runtime implementers can verify compatibility themselves, and
self-report a compatibility level.

# Bronze

* All Node.js core APIs (i.e. functionality exposed by the `lib` directory) are
  supported, unless they're specifically called out in higher support levels.
* They can be loaded via require or ESM import, with `node:*` being supported
  the same way Node.js does (i.e. optional or not, module-by-module).
* Node-API is support for native addons.
* Node.js core tests for all of the above pass, altered as necessary to work in
  the target runtime.

# Silver

* All Node.js addons work (this means V8, libuv, and other native APIs are
  supported).
* Loader hooks work, via some mechanism for their inclusion. If a JS API exists
  for enabling them is in this version of Node.js, it must be supported.
* Node.js core tests for all of the above pass, altered as necessary to work in
  the target runtime.

# Gold

* All Node.js command-line options work.
* All Node.js core tests pass, altered only as necessary to change the name of
  the binary.
