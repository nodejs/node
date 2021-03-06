# Changelog

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

### [3.0.3](https://github.com/tapjs/signal-exit/compare/v3.0.2...v3.0.3) (2020-03-26)


### Bug Fixes

* patch `SIGHUP` to `SIGINT` when on Windows ([cfd1046](https://github.com/tapjs/signal-exit/commit/cfd1046079af4f0e44f93c69c237a09de8c23ef2))
* **ci:** use Travis for Windows builds ([007add7](https://github.com/tapjs/signal-exit/commit/007add793d2b5ae3c382512103adbf321768a0b8))

<a name="3.0.1"></a>
## [3.0.1](https://github.com/tapjs/signal-exit/compare/v3.0.0...v3.0.1) (2016-09-08)


### Bug Fixes

* do not listen on SIGBUS, SIGFPE, SIGSEGV and SIGILL ([#40](https://github.com/tapjs/signal-exit/issues/40)) ([5b105fb](https://github.com/tapjs/signal-exit/commit/5b105fb))



<a name="3.0.0"></a>
# [3.0.0](https://github.com/tapjs/signal-exit/compare/v2.1.2...v3.0.0) (2016-06-13)


### Bug Fixes

* get our test suite running on Windows ([#23](https://github.com/tapjs/signal-exit/issues/23)) ([6f3eda8](https://github.com/tapjs/signal-exit/commit/6f3eda8))
* hooking SIGPROF was interfering with profilers see [#21](https://github.com/tapjs/signal-exit/issues/21) ([#24](https://github.com/tapjs/signal-exit/issues/24)) ([1248a4c](https://github.com/tapjs/signal-exit/commit/1248a4c))


### BREAKING CHANGES

* signal-exit no longer wires into SIGPROF
