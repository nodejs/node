You may also need to check https://chromium.googlesource.com/chromiumos/docs/+/master/glossary.md.

* ABI: [Application Binary Interface](https://en.wikipedia.org/wiki/Application_binary_interface)
* ASAP: As Soon As Possible.
* BE: Big Endian - Byte order style in a multibyte data.
  (Opposite of LE, Little Endian)
* CI: [Continuous Integration](https://en.wikipedia.org/wiki/Continuous_integration).
* CITGM: Canary In The Gold Mine - a smoke test unit in the repo that
  contain popular npm modules.
* CJS: Common JS.
* CLDR: [Common Locale Data Repository](https://en.wikipedia.org/wiki/Common_Locale_Data_Repository).
* CLI: [Command Line Interface](https://en.wikipedia.org/wiki/Command-line_interface).
* CVE: Common Vulnerebilities and Exposures - A database that
  maintains reported security exposures.
* ECMA: European Computer Manufacturers Association - A body that
  governs JS spec among other things.
* EOF: [End Of File](https://en.wikipedia.org/wiki/End-of-file)
* EOL: End Of Line (when used within a program), End of Life
  (when used within project documents).
* ESM: ECMA Script Module.
* ETW: Event Tracing for Windows.
* FFDC: First Failure Data Capture - Common terms for logs, traces
  and dumps that are produced by default on program error.
* FIPS:  Federal Information Processing Standards.
* FS: File System.
* ICU: International Components for Unicode.
* IPC: Inter Process Communication.
* JIT: [Just In Time](https://en.wikipedia.org/wiki/Just-in-time_compilation)
  Is a way of executing computer code that involves compilation during execution
  of a program `at run time` rather than before execution.
* JS/C++ boundary: It refers  to the boundary between V8's runtime and the
  JS code execution which are commonly crossed when invoking JS functions with
  C++ linkage (e.g. interceptors). The boundary could very well exist when we
  are reaching into the C++ to do something that has nothing to do with the
  platform, e.g. URL parsing.
* LGTM: Looks good to me - commonly used to approve a code review.
* LTS: Long Term Support.
* MDN: [Mozila Development Network](https://developer.mozilla.org/en-US).
* OOB: Out Of Bounds - Used in the context of array(memory) access.
* OOM: Out Of Memory.
* PPC: Power PC.
* PTAL: Please Take A Look.
* RAII: [Resource Aquisition Is Intialization](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization)
* REPL: Read Evaluate Print Loop. REPL is available as a standalone program
  (invoke node without arguments), used for developing / testing small programs.
* RFC: Request For Comments.
* RSLGTM: Rubber-Stamp Looks Good To Me. The reviewer approving without doing
  a full code review.
* RSS: [Resident Set Size](https://en.wikipedia.org/wiki/Resident_set_size).
* SMP: Symmetric Multi Processor.
* TSC: Technical Steering Committee. Detailed info see
  [TSC](./GOVERNANCE.md#technical-steering-committee).
* V8: JavaScript engine that powers Node.js and chrome browser.
* VM: The [VM module](https://nodejs.org/api/vm.html) provides a way of
  executing JavaScript on a virtual machine.
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
