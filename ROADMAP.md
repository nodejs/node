# Node.js Roadmap

***This is a living document, it describes the policy and priorities as they exist today but can evolve over time.***

## Stability Policy

The most important consideration in every code change is the impact it will have, positive or negative, on the ecosystem (modules and applications).

Node.js does not remove stdlib JS API.

Shipping with current and well supported dependencies is the best way to ensure long term stability of the platform.

Node.js will continue to adopt new V8 releases.
* When V8 ships a breaking change to their C++ API that can be handled by [`nan`](https://github.com/nodejs/nan)
the *minor* version of Node.js will be increased.
* When V8 ships a breaking change to their C++ API that can NOT be handled by [`nan`](https://github.com/nodejs/nan)
the *major* version of Node.js will be increased.
* When new features in the JavaScript language are introduced by V8 the
*minor* version number will be increased. TC39 has stated clearly that no
backwards incompatible changes will be made to the language so it is
appropriate to increase the minor rather than major.

No new API will be added in *patch* releases.

Any API addition will cause an increase in the *minor* version.

## Channels

Channels are points of collaboration with the broader community and are not strictly scoped to a repository or branch.

* Release - Stable production ready builds. Unique version numbers following semver.
* Canary - Nightly builds w/ V8 version in Chrome Canary + changes landing to Node.js. No version designation.
* NG - "Next Generation." No version designation.

## NG (Next Generation)

In order for Node.js to stay competitive we need to work on the next generation of the platform which will more accurately integrate and reflect the advancements in the language and the ecosystem.

While this constitutes a great leap forward for the platform we will be making this leap without breaking backwards compatibility with the existing ecosystem of modules.

## Immediate Priorities

### Debugging and Tracing

Debugging is one of the first things from everyone's mouth, both developer and enterprise, when describing trouble they've had with Node.js.

The goal of Node.js' effort is to build a healthy debugging and tracing ecosystem and not to try and build any "silver bullet" features for core (like the domains debacle).

The [Tracing WG](https://github.com/nodejs/tracing-wg) is driving this effort:

* AsyncWrap improvements - basically just iterations based on feedback from people using it.
* async-listener - userland module that will dogfood AsyncWrap as well as provide many often requested debugging features.
* Tracing
  * Add tracing support for more platforms (LTTng, etc).
  * [Unify the Tracing endpoint](https://github.com/nodejs/node/issues/729).
  * New Chrome Debugger - Google is working on a version of Chrome's debugger that is without Chrome and can be used with Node.js.

### Ecosystem Automation

In order to maintain a good release cadence without harming compatibility we must do a better job of understanding exactly what impact a particular change or release will have on the ecosystem. This requires new automation.

The initial goals for this automation are relatively simple but will create a baseline toolchain we can continue to improve upon.

* Produce a list of modules that no longer build between two release versions.
* Produce a list of modules that use a particular core API.
* Produce detailed code coverage data for the tests in core.

### Improve Installation and Upgrades

* Host and maintain registry endpoints (Homebrew, apt, etc).
* Document installation and upgrade procedures with an emphasis on using nvm or nave for development and our registry endpoints for traditional package managers and production.

### Streams

* Fix all existing compatibility issues.
* Simplify stream creation to avoid user error.
* Explore and identify compatibility issues with [WHATWG Streams](https://github.com/whatwg/streams).
* Improve stream performance.

### Internationalization / Localization

* Build documentation tooling with localization support built in.
* Reduce size of ICU and ship with it by default.
* Continue growth of our i18n community.
