# Single executable applications

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental: This feature is currently being designed and will
> still change.

<!-- source_link=lib/internal/main/single_executable_application.js -->

Node.js supports the creation of [single executable applications][] by allowing
the injection of a JavaScript file into the binary. During start up, the program
checks if a resource (on [PE][]) or section (on [Mach-O][]) or note (on [ELF][])
named `NODE_JS_CODE` exists. If it is found, it executes its contents, otherwise
it operates like plain Node.js.

This feature allows the distribution of a Node.js application conveniently to a
system that does not have Node.js installed.

A bundled JavaScript file can be turned into a single executable application
with any other tool which can inject resources into the Node.js binary. The tool
should also search the binary for the
`NODE_JS_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0` fuse string and flip the last
character to `1` to indicate that a resource has been injected.

One such tool is [postject][]:

```console
$ cat hello.js
console.log(`Hello, ${process.argv[2]}!`);
$ cp $(command -v node) hello
$ npx postject hello NODE_JS_CODE hello.js --sentinel-fuse NODE_JS_FUSE_fce680ab2cc467b6e072b8b5df1996b2:0
$ ./hello world
Hello, world!
```

This currently only supports running a single embedded [CommonJS][] file.

[CommonJS]: modules.md#modules-commonjs-modules
[ELF]: https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
[Mach-O]: https://en.wikipedia.org/wiki/Mach-O
[PE]: https://en.wikipedia.org/wiki/Portable_Executable
[postject]: https://github.com/nodejs/postject
[single executable applications]: https://github.com/nodejs/single-executable
