OpenSSL Documentation Policy
============================

This document describes the code documentation and commenting requirements
for the OpenSSL project.

The project's documentation is about making the libraries and tools more
accessible to our users and making the code more maintainable. This policy
applies to new submissions; existing code does not uniformly conform to it
and will be brought up to standard gradually.

Any non-trivial change to existing code must bring the affected code into
conformance with this policy as part of the same change. In particular,
renaming or relocating functions, changes to public APIs, and any change
that would render an existing POD page or in-source comment inaccurate
require the corresponding documentation to be updated. This includes
adding documentation that was previously absent where the change brings
the affected code within the scope of this policy.

The form and style of code comments themselves -- comment markers, layout,
the use of `/**` and `/*-` blocks, doxygen markup, the structure of the
sample multi-line comment, and similar -- are described in
[STYLE.md](STYLE.md). This file describes what *must* be documented and
where; [STYLE.md](STYLE.md) describes how code comments look.

Command line commands and arguments
-----------------------------------

All new commands, as well as new or modified arguments to existing
commands, must be documented in the `doc/man1` directory. This
documentation is in POD format.

Public symbols in the libraries
-------------------------------

All new public symbols must be documented in a POD manual page in the
`doc/man3` directory. This includes types, macros, and functions.

The allowed exceptions are:

- guard macros preventing a header file being included twice
- new symbols generated automatically via `make update` (errors, objects, etc.)

Each public function's declaration in its public header must carry a
doxygen comment block. The block's `@see` must include the function's
own manual page (`name(3)`) and may include additional manual pages
that a caller needs to use the function correctly. The doxygen block
is a navigation aid pointing to the canonical reference documentation
in the corresponding POD file; see [STYLE.md](STYLE.md) for the
doxygen form.

Overviews, conventions, et al
-----------------------------

Where additional user-facing information is required, it should be
included in the `doc/man7` section. This includes, but is not limited to:

- algorithm descriptions and parameters
- architectural and subsystem overviews
- user guides and tutorials
- conventions and reference material (environment variables, glossary,
  threading rules, file format conventions)

Internal functions, structures, globals and macros
--------------------------------------------------

Internal functions, structures, globals and macros are non-public
items declared in any header that is not part of the public API.
These include items declared in:

- `include/internal/` (shared across subsystems);
- `include/crypto/` (cryptographic internals);
- per-directory local headers (for example, `crypto/asn1/asn1_local.h`)
  shared between source files in a single subdirectory.

These should all be documented at the declaration site -- that is,
in the header that declares them -- using a doxygen-style comment
block. For functions, this places the comment at the prototype,
where editor tooling (clangd and similar) can surface it to readers
at every call site. The comment should describe the purpose and,
for functions, the input and output arguments and the return value.
See [STYLE.md](STYLE.md) for the doxygen conventions used by OpenSSL.

For *trivial* items, where their operation is obvious from their
implementation, the documentation requirement is not mandated. The
following are generally representative of trivial items, however it is
quite possible for any of these to be non-trivial in specific instances
and therefore require documentation:

- `OSSL_DISPATCH` tables
- upref functions
- free functions
- simple getter/setter functions
- wrappers for other functions (a function that calls a more recent
  `_ex` variant or a group of functions that call a common internal
  routine)

For structures, each of the fields should be commented stating its
purpose. Again, a *trivial* exception applies where the purpose is
obvious. Some representative examples:

- `OSSL_LIB_CTX *ctx;` where there is only one library context referenced
  in the structure.
- `struct *next;` in a linked list implementation.
- `CRYPTO_REF_COUNT refcnt;`

File-local items
----------------

These are functions, structures, globals, and macros that are local
to a single C file: `static` functions, file-scope variables,
structures, and macros defined inside a `.c` file with no declaration
in any header.

These should all be documented at the point of definition. Follow the
same rules and exceptions as for internal items above. In some cases
slightly more leniency with respect to *trivial* can be tolerated.

Code comments
-------------

The form, style, and content guidance for code comments are described in
[STYLE.md](STYLE.md). Comments are required at the points described in
the internal and static sections above, subject to the *trivial*
exception, and at the additional points described in
[STYLE.md](STYLE.md).

Assembly code
-------------

Assembly code should include a good description of the algorithm and
approach being used. This should be followed by a performance comparison
and then the assembly code itself. The assembly code should be well
commented, but it is not necessary to comment every line. A comment
describing each block of code suffices.

For pure-assembly modules (`.s` files and the perlasm scripts that
generate them), comments use the native syntax of the assembler or
generator (typically `#`). Doxygen-style markup does not apply here;
the algorithm description, performance comparison, and per-block
comments described above are still required.

For assembly that appears inline inside a C file (within an `asm()`
statement, for example), the surrounding C function is documented
with doxygen-style C comments as for any other C code; see
[STYLE.md](STYLE.md). Comments inside the `asm()` body itself use
plain C `/* */` comments.

There are no *trivial* exceptions for assembly code.

Configure options
-----------------

New options added to the configuration scripts must be documented in the
[INSTALL.md](INSTALL.md) file.

Changes and news
----------------

Significant modifications should be documented in the
[CHANGES.md](CHANGES.md) file.

Very significant features and changes should be documented in the
[NEWS.md](NEWS.md) file.

In both cases, the added note should be short and to the point, and
should be written for users of the library, focusing on impact rather
than implementation details.

Automated sanity checking
-------------------------

The `make doc-nits` command should be run before submitting a pull
request and any problems it locates must be addressed.

Language
--------

The language used for documentation shall be *British English*.

In general the language, abbreviations, layout and formatting should also
correspond to the
[LDP](https://openssl-library.org/policies/general/glossary/#ldp)
guidelines.

Common sense
------------

Comments and documentation are to improve readability and comprehension.
Where the code is obvious, there is no need to include a comment.
However, common sense applies: always err in favour of including more
comments than less or none. Code that you have just written that is
*obvious* will not necessarily be to someone else two years later. See
[STYLE.md](STYLE.md) for the form and content of code comments.
