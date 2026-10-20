# @perseveranza-pets/milo

Milo is a fast and embeddable HTTP/1.1 parser written in [Rust][rust].

Milo can report parser activity through callbacks or through parser-owned event buffers. See the language-specific API docs for event decoding details: [JavaScript](./docs/js.md), [Rust](./docs/rust.md), and [C++](./docs/cpp.md).

## Support Matrix

Milo supports strict HTTP/1.1 message parsing for Rust, C++, and JavaScript via WebAssembly.

| Target     | Status    | Artifact                                      |
| ---------- | --------- | --------------------------------------------- |
| Rust       | Supported | `milo` crate                                  |
| C++        | Supported | `milo.h` and `libmilo.a`                      |
| JavaScript | Supported | `@perseveranza-pets/milo` WebAssembly package |
| CLI        | Supported | `milo-parser` binary                          |

Milo intentionally rejects HTTP/0.9, HTTP/1.0, RTSP, obs-fold, bare LF, and bare CR. It does not parse HTTP/2 messages, except for strict `PRI * HTTP/2.0` switch-over detection.

For security and ambiguity reduction, Milo rejects request bodies on `GET` and `HEAD`. Responses to `HEAD` are application context: callers must use `skip_body` when they know a response has no body because it belongs to a `HEAD` request.

Use `max_body_payload` when an integration needs to cap body payload consumed by a single parse call. `0` means unlimited. Reaching the limit returns normally with unconsumed input left for the next parse call.

Use `suspend_after_headers` when an integration needs to inspect headers before body parsing starts. Parsing returns after consuming the header terminator and can continue with the next parse call.

For full parser scope, strictness, and protocol behavior, see [Milo Design](./docs/design.md).

## How to use it (JavaScript via WebAssembly)

Install it from npm:

```
npm install @perseveranza-pets/milo
```

For CommonJS projects, install the CJS-only package instead:

```
npm install @perseveranza-pets/milo-cjs
```

Then create a sample source file:

```javascript
import { setup } from '@perseveranza-pets/milo'

/*
  Milo works using callbacks.

  All callbacks have the same signature, which characterizes the payload:

    * The current parser
    * from: The payload offset.
    * size: The payload length.

  The payload parameters above are relative to the last data sent to the milo.parse method.

  If the current callback has no payload, both values are set to 0.

  The callbacks must be provided using setup and are named in snake case.
*/
const milo = setup({
  on_data (p, from, size) {
    console.log(`Pos=${from} Body: ${message.slice(from, from + size).toString()}`)
  }
})

// Prepare a message to parse.
const message = Buffer.from('HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc')

// Allocate a memory in the WebAssembly space. This speeds up data copying to the WebAssembly layer.
const ptr = milo.alloc(message.length)

// Create a buffer we can use normally.
const buffer = Buffer.from(milo.memory.buffer, ptr, message.length)

// Create the parser.
const parser = milo.create()

// Toggle on the callbacks you want to receive
milo.setActiveCallbacks(parser, milo.CALLBACK_ACTIVE_ON_DATA)

// Now perform the main parsing using milo.parse. The method returns the number of consumed characters.
buffer.set(message, 0)
milo.parse(parser, ptr, message.length)

// Cleanup used resources.
milo.destroy(parser)
milo.dealloc(ptr, message.length)
```

The default JavaScript entry point uses the SIMD WebAssembly build. Use `@perseveranza-pets/milo/no-simd` when SIMD is not available, and add `/unbundled` to either entry point to load the external `.wasm` file instead of the bundled JavaScript module.

The WebAssembly release build uses immediate-abort panics: panics trap without unwinding or rich panic messages. The debug build also enables the `on_state_change` callback and provides more detailed WebAssembly errors.

CommonJS projects can use the same entry points from `@perseveranza-pets/milo-cjs`:

```javascript
const { setup } = require('@perseveranza-pets/milo-cjs')
```

Finally build and execute it using `node`:

```bash
node index.js
# Pos=38 Body: abc
```

## How to use it (Rust)

Add `milo-parser` to your `Cargo.toml`:

```toml
[package]
name = "milo-example"
version = "0.1.0"
edition = "2024"
publish = false

[dependencies]
milo-parser = "0.4.0"
```

Create a sample source file:

```rust
use core::ffi::c_void;
use core::slice;

use milo_parser::{Parser, CALLBACK_ACTIVE_ON_DATA};

fn main() {
  // Create the parser.
  let mut parser = Parser::new();

  // Prepare a message to parse.
  let message = String::from("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc");
  parser.context = message.as_ptr() as *mut c_void;

  // Milo works using callbacks.
  //
  // All callbacks have the same signature, which characterizes the payload:
  //
  // p: The current parser.
  // from: The payload offset.
  // size: The payload length.
  //
  // The payload parameters above are relative to the last data sent to the parse
  // method.
  //
  // If the current callback has no payload, both values are set to 0.
  parser.callbacks.on_data = |p: &mut Parser, from: usize, size: usize| {
    let message =
      unsafe { std::str::from_utf8_unchecked(slice::from_raw_parts(p.context.add(from) as *const u8, size)) };

    // Use the callback data.
    println!("Pos={} Body: {}", from, message);
  };

  // Toggle on the callbacks you want to receive
  parser.active_callbacks |= CALLBACK_ACTIVE_ON_DATA;

  // Now perform the main parsing using milo.parse. The method returns the number
  // of consumed characters.
  parser.parse(message.as_ptr(), message.len());
}

```

Finally build and execute it using `cargo`:

```bash
cargo run
# Pos=38 Body: abc
```

## How to use it (C++)

First, let's download Milo release from GitHub.

You will need a static library file (for Linux/Unix/MacOS is `libmilo.a`) and a header file (`milo.h`).

Create a sample source file:

```cpp
#include "milo.h"
#include <cinttypes>
#include <cstdio>
#include <cstring>

int main() {
  // Create the parser.
  milo_parser::Parser* parser = milo_parser::milo_create();

  // Prepare a message to parse.
  const char* message = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc";

  parser->context = (char*) message;

  /*
    Milo works using callbacks.

    All callbacks have the same signature, which characterizes the payload:

      * p: The current parser.
      * from: The payload offset.
      * size: The payload length.

    The payload parameters above are relative to the last data sent to the milo_parse method.

    If the current callback has no payload, both values are set to 0.
  */
  parser->callbacks.on_data = [](milo_parser::Parser* p, uintptr_t from, uintptr_t size) {
    const char* payload = reinterpret_cast<const char*>(p->context) + from;

    printf("Pos=%" PRIuPTR " Body: %.*s\n", from, static_cast<int>(size), payload);
  };

  // Toggle on the callbacks you want to receive
  parser->active_callbacks |= milo_parser::CALLBACK_ACTIVE_ON_DATA;

  // Now perform the main parsing using milo.parse. The method returns the number of consumed characters.
  milo_parser::milo_parse(parser, reinterpret_cast<const unsigned char*>(message), strlen(message));

  // Cleanup used resources.
  milo_parser::milo_destroy(parser);
}
```

And then you can compile using your preferred build system. For instance, let's try with [Clang]:

```bash
clang++ -std=c++11 -o example main.cc libmilo.a
./example
# Pos=38 Body: abc
```

## How to use it (CLI)

Install it from crates.io:

```bash
cargo install milo-parser
```

The `milo-parser` binary reads an HTTP/1.1 message from standard input by default and prints the callbacks emitted by Milo:

```bash
printf 'GET / HTTP/1.1\r\nHost: example.com\r\n\r\n' | milo-parser
# offset=0 size=0 event=request
# offset=0 size=0 event=message_start
# offset=0 size=0 event=state_change
# offset=0 size=3 event=method
```

Each output line uses this format:

```text
offset=$OFFSET size=$SIZE event=$EVENT
```

Parser errors are emitted as callback lines too:

```text
offset=$OFFSET size=$SIZE event=error error=$ERROR description="$DESCRIPTION"
```

Use `-f` or `--file` to parse a file:

```bash
milo-parser --file request.http
```

By default Milo autodetects request or response input. Use `-o` or `--request` to force request parsing, and `-i` or `--response` to force response parsing:

```bash
milo-parser --request --file request.http
milo-parser --response --file response.http
```

## API

See the following files, according to the language you are using:

- [JavaScript via WebAssembly API](./docs/js.md)
- [Rust API](./docs/rust.md)
- [C++ API](./docs/cpp.md)

## Strictness Differences From llhttp

Milo does not aim for byte-for-byte llhttp compatibility when lenient behavior would conflict with Milo's strict parser policy.

- Milo rejects HTTP/0.9, HTTP/1.0, RTSP, obs-fold, bare LF, and bare CR.
- Milo rejects normal HTTP/2 request and response messages.
- Milo rejects request bodies on `GET` and `HEAD`.
- Milo requires `skip_body` for application-known no-body response contexts such as responses to `HEAD`.
- Milo validates protocol framing and syntax, but leaves application semantics to callers.

## Security Boundaries

Milo validates HTTP/1.1 syntax, message framing, protocol switching, connection management, and data-after-close behavior. Milo does not validate routing, authorization, representation semantics, URI normalization, `Host` policy, method-specific request-target forms, CONNECT authority-form, or full header field semantics unless they affect safe framing.

## How it works?

Milo is a simple state machine which copies data in only one optional case: automatically handling the unconsumed portion of the input data.

In all other cases, no data is copied and the memory footprint is very small as only a few dozen `bool`, `uintptr_t`, or `uint64_t` fields can represent the entire parser state.

## Why?

The scope of Milo is to replace [llhttp] as [Node.js] main HTTP parser.

This project aims to:

- Make it maintainable and verifiable using easy to read Rust code.
- Be performant by avoiding any unnecessary data copy.
- Be self-contained and dependency-free.

To see the rationale behind the replacement of llhttp, check Paolo's talk at [Vancouver's Node Collab Summit][vancouver-talk] in January 2023 ([slides][vancouver-slides]).

To see the initial disclosure of milo, check Paolo's talk at [NodeConf EU 2023][nodeconf-talk] in November 2023 ([slides][slides]).

## Sponsored by

[![NearForm](https://raw.githubusercontent.com/ShogunPanda/milo/main/docs/nearform.jpg)][nearform]

## Contributing to milo

See [AGENTS.md](./AGENTS.md) for development setup, local and Docker builds, tests, and contribution guidelines.

## Copyright

Copyright (C) 2023 and above Paolo Insogna (paolo@cowtech.it) and NearForm (https://nearform.com).

Licensed under the ISC license, which can be found at https://choosealicense.com/licenses/isc or in the [LICENSE.md](./LICENSE.md) file.

[rust]: https://www.rust-lang.org/
[webassembly]: https://webassembly.org/
[nearform]: https://nearform.com
[llhttp]: https://github.com/nodejs/llhttp
[Node.js]: https://nodejs.org
[vancouver-talk]: https://youtube.com/watch?v=L-VONzXQ944
[vancouver-slides]: https://talks.cowtech.it/http-parser
[nodeconf-talk]: https://youtube.com/watch?v=dcHbAeO_ccY
[slides]: https://talks.paoloinsogna.dev/milo
[isc]: https://choosealicense.com/licenses/isc
[match]: https://doc.rust-lang.org/rust-by-example/flow_control/match.html
[match-slice]: https://doc.rust-lang.org/rust-by-example/flow_control/match/destructuring/destructure_slice.html
[Clang]: https://clang.llvm.org/
