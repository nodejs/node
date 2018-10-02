# ABI Stability

## Introduction
An Application Binary Interface (ABI) is a way for programs to call functions
and use data structures from other compiled programs. It is the compiled version
of an Application Programming Interface (API). In other words, the headers files
describing the classes, functions, data structures, enumerations, and constants
which enable an application to perform a desired task correspond by way of
compilation to a set of addresses and expected parameter values and memory
structure sizes and layouts with which the provider of the ABI was compiled.

The application using the ABI must be compiled such that the available
addresses, expected parameter values, and memory structure sizes and layouts
agree with those with which the ABI provider was compiled. This is usually
accomplished by compiling against the headers provided by the ABI provider.

Since the provider of the ABI and the user of the ABI may be compiled at
different times with different versions of the compiler, a portion of the
responsibility for ensuring ABI compatibility lies with the compiler. Different
versions of the compiler, perhaps provided by different vendors, must all
produce the same ABI from a header file with a certain content, and must produce
code for the application using the ABI that accesses the API described in a
given header according to the conventions of the ABI resulting from the
description in the header. Modern compilers have a fairly good track record of
not breaking the ABI compatibility of the applications they compile.

The remaining responsibility for ensuring ABI compatibility lies with the team
maintaining the header files which provide the API that results, upon
compilation, in the ABI that is to remain stable. Changes to the header files
can be made, but the nature of the changes has to be closely tracked to ensure
that, upon compilation, the ABI does not change in a way that will render
existing users of the ABI incompatible with the new version.

## ABI Stability in Node.js
Node.js provides header files maintained by several independent teams. For
example, header files such as `node.h` and `node_buffer.h` are maintained by
the Node.js team. `v8.h` is maintained by the V8 team, which, although in close
co-operation with the Node.js team, is independent, and with its own schedule
and priorities. Thus, the Node.js team has only partial control over the
changes that are introduced in the headers the project provides. As a result,
the Node.js project has adopted [semantic versioning](https://semver.org/).
This ensures that the APIs provided by the project will result in a stable ABI
for all minor and patch versions of Node.js released within one major version.
In practice, this means that the Node.js project has committed itself to
ensuring that a Node.js native addon compiled against a given major version of
Node.js will load successfully when loaded by any Node.js minor or patch version
within the major version against which it was compiled.

## N-API
Demand has arisen for equipping Node.js with an API that results in an ABI that
remains stable across multiple Node.js major versions. The motivation for
creating such an API is as follows:
* The JavaScript language has remained compatible with itself since its very
early days, whereas the ABI of the engine executing the JavaScript code changes
with every major version of Node.js. This means that applications consisting of
Node.js packages written entirely in JavaScript need not be recompiled,
reinstalled, or redeployed as a new major version of Node.js is dropped into
the production environment in which such applications run. In contrast, if an
application depends on a package that contains a native addon, the application
has to be recompiled, reinstalled, and redeployed whenever a new major version
of Node.js is introduced into the production environment. This disparity
between Node.js packages containing native addons and those that are written
entirely in JavaScript has added to the maintenance burden of production
systems which rely on native addons.

* Other projects have started to produce JavaScript interfaces that are
essentially alternative implementations of Node.js. Since these projects are
usually built on a different JavaScript engine than V8, their native addons
necessarily take on a different structure and use a different API. Nevertheless,
using a single API for a native addon across different implementations of the
Node.js JavaScript API would allow these projects to take advantage of the
ecosystem of JavaScript packages that has accrued around Node.js.

* Node.js may contain a different JavaScript engine in the future. This means
that, externally, all Node.js interfaces would remain the same, but the V8
header file would be absent. Such a step would cause the disruption of the
Node.js ecosystem in general, and that of the native addons in particular, if
an API that is JavaScript engine agnostic is not first provided by Node.js and
adopted by native addons.

To these ends Node.js has introduced N-API in version 8.6.0 and marked it as a
stable component of the project as of Node.js 8.12.0. The API is defined in the
headers [`node_api.h`][] and [`node_api_types.h`][], and provides a forward-
compatibility guarantee that crosses the Node.js major version boundary. The
guarantee can be stated as follows:

**A given version *n* of N-API will be available in the major version of
Node.js in which it was published, and in all subsequent versions of Node.js,
including subsequent major versions.**

A native addon author can take advantage of the N-API forward compatibility
guarantee by ensuring that the addon makes use only of APIs defined in
`node_api.h` and data structures and constants defined in `node_api_types.h`.
By doing so, the author facilitates adoption of their addon by indicating to
production users that the maintenance burden for their application will increase
no more by the addition of the native addon to their project than it would by
the addition of a package written purely in JavaScript.

N-API is versioned because new APIs are added from time to time. Unlike
semantic versioning, N-API versioning is cumulative. That is, each version of
N-API conveys the same meaning as a minor version in the semver system, meaning
that all changes made to N-API will be backwards compatible. Additionally, new
N-APIs are added under an experimental flag to give the community an opportunity
to vet them in a production environment. Experimental status means that,
although care has been taken to ensure that the new API will not have to be
modified in an ABI-incompatible way in the future, it has not yet been
sufficiently proven in production to be correct and useful as designed and, as
such, may undergo ABI-incompatible changes before it is finally incorporated
into a forthcoming version of N-API. That is, an experimental N-API is not yet
covered by the forward compatibility guarantee.

[`node_api.h`]: ../../src/node_api.h
[`node_api_types.h`]: ../..src/node_api_types.h
