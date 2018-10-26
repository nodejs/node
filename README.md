This is the official [V8](https://github.com/v8/v8/wiki) fork of [Node.js](https://github.com/nodejs/node) with a recent V8 version.

To build from source, run
```console
git clone https://github.com/v8/node v8-node
cd v8-node
./configure --build-v8-with-gn
make -j4 node
```

Or download the latest build for Ubuntu from this [build bot](https://build.chromium.org/p/client.v8.fyi/builders/V8%20-%20node.js%20integration). Select a build, then use *Archive link download*.

To check the V8 version in Node, have a look at [`v8-version.h`](https://github.com/v8/node/blob/vee-eight-lkgr/deps/v8/include/v8-version.h) or run
```console
node -e "console.log(process.versions.v8)"
```
