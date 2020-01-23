# llhttp
[![Build Status](https://secure.travis-ci.org/nodejs/llhttp.svg)](http://travis-ci.org/nodejs/llhttp)

Port of [http_parser][0] to [llparse][1].

## Why?

Let's face it, [http_parser][0] is practically unmaintainable. Even
introduction of a single new method results in a significant code churn.

This project aims to:

* Make it maintainable
* Verifiable
* Improving benchmarks where possible

More details in [Fedor Indutny's talk at JSConf EU 2019](https://youtu.be/x3k_5Mi66sY)

## How?

Over time, different approaches for improving [http_parser][0]'s code base
were tried. However, all of them failed due to resulting significant performance
degradation.

This project is a port of [http_parser][0] to TypeScript. [llparse][1] is used
to generate the output C and/or bitcode artifacts, which could be compiled and
linked with the embedder's program (like [Node.js][7]).

## Performance

So far llhttp outperforms http_parser:

|                 | input size |  bandwidth   |  reqs/sec  |   time  |
|:----------------|-----------:|-------------:|-----------:|--------:|
| **llhttp** _(C)_ | 8192.00 mb | 1777.24 mb/s | 3583799.39 ops/sec | 4.61 s |
| **http_parser** | 8192.00 mb | 694.66 mb/s | 1406180.33 req/sec | 11.79 s |

llhttp is faster by approximately **156%**.

## Maintenance

llhttp project has about 1400 lines of TypeScript code describing the parser
itself and around 450 lines of C code and headers providing the helper methods.
The whole [http_parser][0] is implemented in approximately 2500 lines of C, and
436 lines of headers.

All optimizations and multi-character matching in llhttp are generated
automatically, and thus doesn't add any extra maintenance cost. On the contrary,
most of http_parser's code is hand-optimized and unrolled. Instead describing
"how" it should parse the HTTP requests/responses, a maintainer should
implement the new features in [http_parser][0] cautiously, considering
possible performance degradation and manually optimizing the new code.

## Verification

The state machine graph is encoded explicitly in llhttp. The [llparse][1]
automatically checks the graph for absence of loops and correct reporting of the
input ranges (spans) like header names and values. In the future, additional
checks could be performed to get even stricter verification of the llhttp.

## Usage

```C
#include "llhttp.h"

llhttp_t parser;
llhttp_settings_t settings;

/* Initialize user callbacks and settings */
llhttp_settings_init(&settings);

/* Set user callback */
settings.on_message_complete = handle_on_message_complete;

/* Initialize the parser in HTTP_BOTH mode, meaning that it will select between
 * HTTP_REQUEST and HTTP_RESPONSE parsing automatically while reading the first
 * input.
 */
llhttp_init(&parser, HTTP_BOTH, &settings);

/* Parse request! */
const char* request = "GET / HTTP/1.1\r\n\r\n";
int request_len = strlen(request);

enum llhttp_errno err = llhttp_execute(&parser, request, request_len);
if (err == HPE_OK) {
  /* Successfully parsed! */
} else {
  fprintf(stderr, "Parse error: %s %s\n", llhttp_errno_name(err),
          parser.reason);
}
```

---

#### LICENSE

This software is licensed under the MIT License.

Copyright Fedor Indutny, 2018.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

[0]: https://github.com/nodejs/http-parser
[1]: https://github.com/nodejs/llparse
[2]: https://en.wikipedia.org/wiki/Register_allocation#Spilling
[3]: https://en.wikipedia.org/wiki/Tail_call
[4]: https://llvm.org/docs/LangRef.html
[5]: https://llvm.org/docs/LangRef.html#call-instruction
[6]: https://clang.llvm.org/
[7]: https://github.com/nodejs/node
