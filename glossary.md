You may also need to check https://chromium.googlesource.com/chromiumos/docs/+/master/glossary.md.

* ABI: [Application Binary Interface](https://en.wikipedia.org/wiki/Application_binary_interface)
* ASAP: As Soon As Possible.
* ASYNC: Asynchronous
* BE: Big Endian - Byte order style in a multibyte data.
  (Opposite of LE, Little Endian)
* CI: Continuous Integration.
* CITGM: Canary In The Gold Mine - a smoke test unit in the repo that
  contain popular npm modules.
* CJS: Common JS.
* CLDR: Common Locale Data Repository.
* CLI: [Command Line Interface](https://en.wikipedia.org/wiki/Command-line_interface)
* CMD: Command.
* CVE: Common Vulnerebilities and Exposures - A database that
  maintains reported security exposures.
* ECMA: European Computer Manufacturers Association - A body that
  governs JS spec among other things.
* ENV: Environment - General term for execution environment.
* EOF: [End Of File](https://en.wikipedia.org/wiki/End-of-file)
* EOL: End Of Line (when used within a program), End of Life
  (when used within project documents).
* ESM: ECMA Script Module.
* ETW: Event Tracing for Windows.
* FD: File Descriptor.
* FFDC: First Failure Data Capture - Common terms for logs, traces
  and dumps that are produced by default on program error.
* FIPS:  Federal Information Processing Standards.
* FS: File System.
* ICU: International Components for Unicode.
* IPC: Inter Process Communication.
* JIT: Just In Time - General term for dynamic compiler in
  managed runtimes.
* JS/C++ boundary: A layer that bridges between JS APIs and the C++
  helpers that implements / abstracts platform capabilities.
* LGTM: Looks good to me - commonly used to approve a code review.
* LTS: Long Term Support.
* MDN: Mozila Development Network - A vast and authentic
  documentation on JavaScript.
* OOB: Out Of Bounds - Used in the context of TCP data transport.
* OOM: Out Of Memory.
* OSX: Mac OS.
* PPC: Power PC.
* PTAL: Please Take A Look.
* RAII: [Resource Aquisition Is Intialization](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization)
* REPL: Read Evaluate Print Loop.
* RFC: Request For Comments.
* RSLGTM: Rubber-Stamp Looks Good To Me. The reviewer approving without doing
  a full code review.
* RSS: Resident Set Size.
* SMP: Symmetric Multi Processor.
* TSC: Technical Steering Committee. Detailed info see
  [TSC](./GOVERNANCE.md#technical-steering-committee).
* V8: JavaScript engine that is embedded in Node.js.
* VM: Virtual Machine, in the context of abstracting instructions,
  not to be confused with H/W level or O/S level Virtual machines.
* WASI: [Web Assemby System Interface]( https://github.com/WebAssembly/WASI)
* WASM: Web Assembly.
* WG: Working Group - autonomous teams in the project with specific
  focus areas.
* WHATWG: Web Hypertext Application Technology Working Group
* WIP: "Work In Progress" - e.g. a patch that's not finished, but
may be worth an early look.
* WPT: [web-platform-tests](https://github.com/web-platform-tests/wpt)
* bootstrap: Early phase in the Node.js process startup - that sets up
  the Node.js execution environment and loads the internal modules.
* code cache: A chunk of bytes storing compiled JS code and its metadata.
* deps: upstream projects that this project depends.
* godbolt: [Compiler Explorer](https://godbolt.org/) run compilers
  interactively from your web browser and interact with the assembly.
  Was created by and is primarily administrated by Matt Godbolt.
* native modules/addons: A module / shared library that is
  implemented in C/C++, but exposes one or more interfaces,
  callable from JS.
* primordials: Pristine built-ins that are not effected by prototype
  pollution and tampering with built-ins.
* snapshot: When referring to the V8 startup snapshot, a chunk of
  bytes containing data serialized from a V8 heap, which can be
  deserialized to restore the states of the V8 engine.
* vendoring: consuming external software into this project.
