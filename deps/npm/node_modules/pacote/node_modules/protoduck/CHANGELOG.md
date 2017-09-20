# Change Log

All notable changes to this project will be documented in this file. See [standard-version](https://github.com/conventional-changelog/standard-version) for commit guidelines.

<a name="4.0.0"></a>
# [4.0.0](https://github.com/zkat/protoduck/compare/v3.3.2...v4.0.0) (2017-04-17)


### Bug Fixes

* **test:** .name is inconsistently available ([3483f4a](https://github.com/zkat/protoduck/commit/3483f4a))


### Features

* **api:** Fresh New API™ ([#2](https://github.com/zkat/protoduck/issues/2)) ([534e5cf](https://github.com/zkat/protoduck/commit/534e5cf))
* **constraints:** added optional where-constraints ([16ad124](https://github.com/zkat/protoduck/commit/16ad124))
* **defaults:** allow default impls without arrays in defs ([6cf7d84](https://github.com/zkat/protoduck/commit/6cf7d84))
* **deps:** use genfun[@4](https://github.com/4) ([f6810a7](https://github.com/zkat/protoduck/commit/f6810a7))
* **meta:** bringing project stuff up to date ([61791da](https://github.com/zkat/protoduck/commit/61791da))


### BREAKING CHANGES

* **api:** The API was significantly overhauled.

* New protocol creating is now through protoduck.define() instead of protoduck()
* Implementations are through Duck#impl instead of Duck(...)
* The `private` option was removed
* Static protocols were removed -- only method-style protocols are available now.
* As part of that: the target argument to impl can no longer be omitted
* The main export object is now the metaobject. protoduck.impl can be used to extend to MOP
* .isDerivable is now a property on Duck instances, not a static method
* .hasImpl is now a method on Duck instances, not a static method
* Protoduck will now genfunnify existing functions as default methods for genfuns declared in a protocol when implementing
* Error messages have been overhauled to be more helpful
* **deps:** nextMethod is now an extra argument to methods
* **meta:** node@<4 is no longer supported
