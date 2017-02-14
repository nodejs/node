# Building Node with Ninja

The purpose of this guide is to show how to build Node.js using [Ninja][], as doing so can be significantly quicker than using `make`. Please see [Ninja's site][Ninja] for installation instructions (unix only).

To build Node with ninja, there are 3 steps that must be taken:

1. Configure the project's OS-based build rules via `./configure --ninja`.
2. Run `ninja -C out/Release` to produce a compiled release binary.
3. Lastly, make symlink to `./node` using `ln -fs out/Release/node node`.

When running `ninja -C out/Release` you will see output similar to the following if the build has succeeded:
```txt
ninja: Entering directory `out/Release`
[4/4] LINK node, POSTBUILDS
```

The bottom line will change while building, showing the progress as `[finished/total]` build steps.
This is useful output that `make` does not produce and is one of the benefits of using Ninja.
Also, Ninja will likely compile much faster than even `make -j4` (or `-j<number of processor threads on your machine>`).

## Considerations

Ninja builds vary slightly from `make` builds. If you wish to run `make test` after, `make` will likely still need to rebuild some amount of Node.

As such, if you wish to run the tests, it can be helpful to invoke the test runner directly, like so:
`tools/test.py --mode=release message parallel sequential -J`

## Alias

`alias nnode='./configure --ninja && ninja -C out/Release && ln -fs out/Release/node node'`

## Producing a debug build

The above alias can be modified slightly to produce a debug build, rather than a release build as shown below:
`alias nnodedebug='./configure --ninja && ninja -C out/Debug && ln -fs out/Debug/node node_g'`


[Ninja]: https://ninja-build.org/
