# Node.js: Embedding in C++ [![Build Status](https://travis-ci.org/hpicgs/node.svg?branch=node_lib)](https://travis-ci.org/hpicgs/node)

This fork proposes changes to node.js' shared library interface to allow for a easier and more flexible development experience when embedding node.js into C++ applications. This project is a work in progress in the "Advanced Development in C++" project seminar at Hasso Platter Institute's Chair of Computer Graphics Systems.

For example applications showing how to use node's current embedding interface, as well as examples for our proposed new interface, please visit [this repository](https://github.com/hpicgs/node-embed).

For information concerning the setup, usage etc. of this project, please read the [wiki](https://github.com/hpicgs/node/wiki) (work in progress).

### Building Node as _shared library_

#### Linux

```
./configure --shared
make -j4
```
