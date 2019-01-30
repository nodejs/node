# Inspector Protocol Encoding Library

### Notice

Code in this directory (including tests and library) is not used in production
yet, and won't be for a little while. Below is a rough sketch as to the overall
plan as well as the goals.

## Objective

We're hoping for:

- Support a binary protocol with a reasonably efficient wire format.
  Efficient means that:
  - We can send .png images and similar bytes over this wire.
    JSON currently requires us to base64 encode such pieces.
  - We can extract message ids etc. from the wire format without
    fully parsing the entire message. With JSON This is not very
    practical because strings etc. are not preceded with their length,
    so one is forced to read through them.
  - Perhaps also, we may want to append session ids and the like to a message.
- Reduce some runtime overhead:
  - Reduce the overhead of conversions between utf8 / utf16 / etc.
    E.g., for keys in dictionaries, we may want to assume they're utf8
    and do this encoding once so that we can efficiently look them up.
  - Reduce the in-memory overhead, that is, don't require round-tripping
    every message via base::Value and similar structures.
- Reduce / maintain code size: The current template approach duplicates some
  compiled code in multiple namespaces; linking against this as a third party
  library should allow us to reduce the code size a bit. E.g., we'll just have
  one copy of the json parser (not a great example because the json parser isn't
  quite used multiple times, I think the linker may eliminate the multiple
  copies already). But overall, compiled code size should come down a bit for
  existing functionality, and we'll try our best to keep the new functionality
  (binary protocol) from adding more code bloat.
- Reduce complexity / make code easier to test / maintain / etc.: It should
  become possible to test the inspector protocol layer prior to rolling it into
  the various consuming projects. Having a library / code that's unittested,
  should help toward this direction. We may also add some integration test,
  e.g. some .pdl file with example protocol to drive the code generator
  all the way to round / tripping / handling some messages.
  The distinction between code that's part of the protocol layer and the other
  parts of the inspector / devtools should become somewhat easier to see.

## Approach

To begin, we wish to make a library for the core part of digesting
inspector_protocol messages - the encoding library, in this directory. That is,
encoding / decoding as well as extracting some values that are needed for
dispatch (method, message id, session id).

Our first input shall be JSON, the existing wire format. It's convenient for
testing, but also we'll later be able to use it to convert the existing wire
format as early as possible into binary, e.g. in the devtools pipe for headless,
and therefore we won't need to maintain much separate code between json and
binary inputs.

With the approach described here, we will not parse the JSON into dictionaries
or base::Value / protocol::Value nested structures. Instead, we'll convert it
into a binary format, probably CBOR.

For this, json_parser{.h,.cc} implements a JSON parser with a SAX style
handler interface, which will drive conversion into a binary buffer via a
specific handler. We'll call the abstraction for the binary format
MessageBuffer from here (most likely we'll have a C++ class with that name).

For parts of the Chromium code that doesn't need to inspect deep into the
inspector_protocol messages, we'll be able to extract individual values (message
id, session id, method name) from this MessageBuffer.  This is because
MessageBuffer owns the wire format bytes (e.g. std::vector<uint_t> in a private
field) and can return values as string pieces or int64_t as appropriate.

This library does not involve code generation - it's just checked in C++. The
library will be unittested, and because we have a JSON parser, it's an excellent
way to drive examples into the CBOR buffer and check that they round trip OK. It
may be practical to develop a JSON serializer in the process as well, at first to
make testing simpler, and then later, to use it for compatibility in production.

## Dependencies

There is strtod (for json), there is character encoding, and there is
string / vector / etc. libraries. We'll try handle them to maximize
portability while making it easy to test the library.

For strtod, clients of the library will need to provide their
implementation. We'll use virtual method dispatch (abstract class +
implementation), they'll have to pass in an object, e.g. a singleton.

For UTF8 / UTF16 Unicode encoding, we'll try to avoid adding dependencies and
see how far we get with the approach, much like jsoncpp does it, basically very
minimal / light and no dependency on ICU. We'll depend on mini_chromium for
testing, so that gives us ICU there.

strings. We'll try to keep this minimal as well, by primarily working with
string pieces on the public surface on the library. E.g., for the message id,
which can be extracted from the binary message, we'll return a string piece
which is backed by our binary buffer.

The above library should be usable at least from the browser side, blink, and
v8, but likely also from other projects (e.g. Google internal).

## Code Generation

The code generator (not part of encoding library) will provide:

* Types for parameters, results, notification objects. These are likely
  generated classes just like now.
* It will be possible to instatiate these types directly from binary messages,
  and it will be possible to directly serialize to binary messages.  This will
  be generated code, somewhat similar to the code that's in protocol buffers
  when ParseFromString / ToString is invoked.  It will probably involve
  supporting return types / parameters in these routines that come from the
  third party library, or if not, we'll instantiate it under the hood. It should
  not involve protocol::Value except maybe for free-form parts of the messages
  ("any" in the .pdl).
* The code generator may provide glue.  E.g., convert between the
  aforementioned string piece with whatever is the appropriate string piece in
  blink / v8 / etc.
* The code generator is responsible for significant parts of the dispatching
  logic.

The code generator is currently implemented in Python / jinja2 templates; we can
probably adapt this / bend it to these needs. The templates for the existing
functionality should shrink, however. E.g., we will not have a json parser in a
template but rather use the one in the new library, and we may also move other
supporting code out of the templates and into the library. The benefit of this
should be that it's easier to understand / test / maintain, especially it should
become possible to unittest within this project and provide sufficient
confidence.
