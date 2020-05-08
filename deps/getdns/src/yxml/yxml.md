% Yxml Manual

# Introduction

Yxml is a small non-validating and mostly conforming XML parser written in C.

The latest version of yxml and this document can be found on
[https://dev.yorhel.nl/yxml](https://dev.yorhel.nl/yxml).

# Compiling yxml

Due to the small size of yxml, the recommended way to use it is to copy the
[yxml.c](https://g.blicky.net/yxml.git/plain/yxml.c) and
[yxml.h](https://g.blicky.net/yxml.git/plain/yxml.h) from the git repository
into your project directory, and compile and link yxml.c as part of your
program or library.

The git repository also includes a Makefile. Running `make` without specifying
a target will compile a `.a` file for easy static linking. A test suite is
available under `make test`.

# API documentation

## Overview

Yxml is designed to be very flexible and efficient, and thus offers a
relatively low-level stream-based API. The entire API consists of two typedefs
and three functions:

```c
typedef enum { /* .. */ } yxml_ret_t;
typedef struct { /* .. */ } yxml_t;

void yxml_init(yxml_t *x, void *buf, size_t bufsize);
yxml_ret_t yxml_parse(yxml_t *x, int ch);
yxml_ret_t yxml_eof(yxml_t *x);
```

The values of _yxml\_ret\_t_ and the public fields of _yxml\_t_ are explained
in detail below. Parsing a file using yxml involves three steps:

1. Initialization, using `yxml_init()`.
2. Parsing. This is performed in a loop where `yxml_parse()` is called on each
   character of the input file.
3. Finalization, using `yxml_eof()`.

## Initialization

```c
#define BUFSIZE 4096
void *buf = malloc(BUFSIZE);
yxml_t x;
yxml_init(&x, buf, BUFSIZE);
```

The parsing state for an input document is remembered in the `yxml_t`
structure. This structure needs to be allocated and initialized before parsing
a new XML document.

Allocating space for the `yxml_t` structure is the responsibility of the
application. Allocation can be done on the stack, but it is also possible to
embed the struct inside a larger object or to allocate space for the struct
separately.

`yxml_init()` takes a pointer to an (uninitialized) `yxml_t` struct as first
argument and performs the necessary initialization. The two additional
arguments specify a pointer to a buffer and the size of this buffer. The given
buffer must be writable, but does not have to be initialized by the
application.

The buffer is used internally by yxml to keep a stack of opened XML element
names, property names and PI targets. The size of the buffer determines both
the maximum depth in which XML elements can be nested and the maximum length of
element names, property names and PI targets. Each name consumes
`strlen(name)+1` bytes in the buffer, and the first byte of the buffer is
reserved for the `\0` byte. This means that in order to parse an XML document
with an element name of 100 bytes, a property name or PI target of 50 bytes and
a nesting depth of 10 levels, the buffer must be at least
`1+10*(100+1)+(50+1)=1062` bytes. Note that properties and PIs don't nest, so
the `max(PI_name, property_name)` only needs to be counted once.

It is not currently possibly to dynamically grow the buffer while parsing, so
it is important to choose a buffer size that is large enough to handle all the
XML documents that you want to parse. Since element names, property names and
PI targets are typically much shorter than in the previous example, a buffer
size of 4 or 8 KiB will give enough headroom even for documents with deep
nesting.

As a useful hack, it is possible to merge the memory for the `yxml_t` struct
and the stack buffer in a single allocation:

```c
yxml_t *x = malloc(sizeof(yxml_t) + BUFSIZE);
yxml_init(x, x+1, BUFSIZE);
```

This way, the complete parsing state can be passed around with a single
pointer, and both the struct and the buffer can be freed with a single call to
`free(x)`.

## Parsing

```c
yxml_t *x; /* An initialized state */
char *doc; /* The XML document as a zero-terminated string */
for(; *doc; doc++) {
  yxml_ret_t r = yxml_parse(x, *doc);
  if(r < 0)
    exit(1); /* Handle error */
  /* Handle any tokens we are interested in */
}
```

The actual parsing of an XML document is facilitated by the `yxml_parse()`
function. It accepts a pointer to an initialized `yxml_t` struct as first
argument and a byte as second argument. The byte is passed as an `int`, and
values in the range of -128 to 255 (both inclusive) are accepted. This way you
can pass either `signed char` or `unsigned char` values, yxml will work fine
with both. To parse a complete document, `yxml_parse()` needs to be called for
each byte of the document in sequence, as done in the above example.

For each byte, `yxml_parse()` will return either _YXML\_OK_ (0), a token (>0)
or an error (<0). _YXML\_OK_ is returned if the given byte has been
parsed/consumed correctly but that otherwise nothing worthy of note has
happened. The application should then continue processing and pass the next
byte of the document.

### Public State Variables

After each call to `yxml_parse()`, a number of interesting fields in the
`yxml_t` struct are updated. The fields documented here are part of the API,
and are considered as extra return values of `yxml_parse()`. All of these
fields should be considered read-only.

`char *elem;`
:   Name of the currently opened XML element. Points into the buffer given to
    `yxml_init()`. Described in ["Elements"](#elements).

`char *attr;`
:   Name of the currently opened attribute. Points into the buffer given to
    `yxml_init()`. Described in ["Attributes"](#attributes).

`char *pi;`
:   Target of the currently opened PI. Points into the buffer given to
    `yxml_init()`. Described in ["Processing Instructions"](#processing-instructions).

`char data[8];`
:   Character data of element contents, attribute values or PI contents. Described
    in ["Character Data"](#character-data).

`uint32_t line;`
:   Number of the line in the XML document that is currently being parsed.

`uint64_t byte;`
:   Byte offset into the current line the XML document.

`uint64_t total;`
:   Byte offset into the XML document.

The values of the _elem_, _attr_, _pi_ and _data_ elements depend on the
parsing context, and only remain valid within that context. The exact contexts
in which these fields contain valid information is described in their
respective sections below.

The _line_, _byte_ and _total_ fields are mainly useful for error reporting.
When `yxml_parse()` reports an error, these fields can be used to generate a
useful error message. For example:

```c
printf("Parsing error at %s:%"PRIu32":%"PRIu64" byte offset %"PRIu64",
  filename, x->line, x->byte, x->total);
```

### Error Handling

Errors are not recoverable. No further calls to `yxml_parse()` or `yxml_eof()`
should be performed on the same `yxml_t` struct. Re-initializing the same
struct using `yxml_init()` to start parsing a new document is possible,
however.  The following error values may be returned by `yxml_parse()`:

YXML\_EREF
:   Invalid character or entity reference. E.g. `&whatever;` or `&#ABC;`.

YXML\_ECLOSE
:   Close tag does not match open tag. E.g. `<Tag> .. </SomeOtherTag>`.

YXML\_ESTACK
:   Stack overflow. This happens when the buffer given to `yxml_init()` was not
    large enough to parse this document. E.g. when elements are too deeply nested
    or an element name, attribute name or PI target is too long.

YXML\_ESYN
:   Miscellaneous syntax error.

## Handling Tokens

The `yxml_parse()` function will return tokens as they are found. When loading
an XML document, it is important to know which tokens are returned in which
situation and how to handle them.

The following graph shows the (simplified) state machine of the parser to
illustrate the order in which tokens are returned. The labels on the edge
indicate the tokens that are returned by `yxml_parse()`, with their `YXML_`
prefix removed.  The special return value `YXML_OK` and error returns are not
displayed.

![](https://dev.yorhel.nl/img/yxml-apistates.png)

Tokens that the application is not interested in can be ignored safely. For
example, if you are not interested in handling processing instructions, then
the `YXML_PISTART`, `YXML_PICONTENT` and `YXML_PIEND` tokens can be handled
exactly as if they were an alias for `YXML_OK`.

### Elements

The `YXML_ELEMSTART` and `YXML_ELEMEND` tokens are returned when an XML
element is opened and closed, respectively. When `YXML_ELEMSTART` is returned,
the _elem_ struct field will hold the name of the element. This field will be
valid (i.e. keeps pointing to the name of the opened element) until the end of
the attribute list. That is, until any token other than those described in
["Attributes"](#attributes) is returned. Although the _elem_ pointer itself may be reused
and modified while parsing the contents of the element, the buffer that _elem_
points to will remain valid up to and including the corresponding
`YXML_ELEMEND`.

Yxml will verify that elements properly nest and that the name of each closing
tag properly matches that of the corresponding opening tag. The application may
safely assume that each `YXML_ELEMSTART` is properly matched with a
`YXML_ELEMEND`, or that otherwise an error is returned. Furthermore, only a
single root element is allowed. When the root element is closed, no further
`YXML_ELEMSTART` tokens will be returned.

No distinction is made between self-closing tags and elements with empty
content. For example, both `<a/>` and `<a></a>` will result in the
`YXML_ELEMSTART` token (with `elem="a"`) followed by `YXML_ELEMEND`.

Element contents are returned in the form of the `YXML_CONTENT` token and the
_data_ field. This is described in more detail in ["Character
Data"](#character-data).

### Attributes

Element attributes are passed using the `YXML_ATTRSTART`, `YXML_ATTRVAL` and
`YXML_ATTREND` tokens. The name of the attribute is available in the _attr_
field, which is available when `YXML_ATTRSTART` is returned and valid up to
and including the next `YXML_ATTREND`.

Yxml does not verify that attribute names are unique within a single element.
It is thus possible that the same attribute will appear twice, possibly with a
different value. The correct way to handle this situation is to stop parsing
the rest of the document and to report an error, but if the application is not
interested in all attributes, detecting duplicates in them may complicate the
code and possibly even introduce security vulnerabilities (e.g. algorithmic
complexity attacks in a hash table). As such, the best solution is to report an
error when you can easily detect a duplicate attribute, but ignore duplicates
that require more effort to be detected.

The attribute value is returned with the `YXML_ATTRVAL` token and the _data_
field. This is described in more detail in ["Character Data"](#character-data).

### Processing Instructions

Processing instructions are passed in similar fashion to attributes, and are
passed using `YXML_PISTART`, `YXML_PICONTENT` and `YXML_PIEND`. The target of
the PI is available in the _pi_ field after `YXML_PISTART` and remains valid up
to (but excluding) the next `YXML_PIEND` token.

PI contents are returned as `YXML_PICONTENT` tokens and using the _data_ field,
described in more detail in ["Character Data"](#character-data).

### Character Data

Element contents (`YXML_CONTENT`), attribute values (`YXML_ATTRVAL`) and PI
contents (`YXML_PICONTENT`) are all passed to the application in small chunks
through the _data_ field. Each time that `yxml_parse()` returns one of these
tokens, the _data_ field will contain one or more bytes of the element
contents, attribute value or PI content. The string is zero-terminated, and its
value is only valid until the next call to `yxml_parse()`.

Typically only a single byte is returned after each call, but multiple bytes
can be returned in the following special cases:

- Character references outside of the ASCII character range. When a character
  reference is encountered in element contents or in an attribute value, it is
  automatically replaced with the referenced character. For example, the XML
  string `&#47;` is replaced with the single character "/". If the character
  value is above 127, its value is encoded in UTF-8 and then returned as a
  multi-byte string in the _data_ field. For example, the character reference
  `&#xe7;` is returned as the C string "\\xc3\\xa9", which is the UTF-8
  encoding for the character "é". Character references are not expanded in PI
  contents.
- The special character "\]" in CDATA sections. When the "\]" character is
  encountered inside a CDATA section, yxml can't immediately return it to the
  application because it does not know whether the character is part of the
  CDATA ending or whether it is still part of its contents. So it remembers the
  character for the next call to `yxml_parse()`, and if it then turns out that
  the character was part of the CDATA contents, it returns both the "\]"
  character and the following byte in the same _data_ string. Similarly, if two
  "\]" characters appear in sequence as part of the CDATA content, then the two
  characters are returned in a single _data_ string together with the byte that
  follows. CDATA sections only appear in element contents, so this does not
  happen in attribute values or PI contents.
- The special character "?" in PI contents. This is similar to the issue with
  "\]" characters in CDATA sections. Yxml remembers a "?" character while
  parsing a PI, and then returns it together with the byte following it if it
  turned out to be part of the PI contents.

Note that `yxml_parse()` operates on bytes rather than characters. If the
document is encoded in a multi-byte character encoding such as UTF-8, then each
Unicode character that occupies more than a single byte will be broken up and
its bytes processed individually. As a result, the bytes returned in the
_data_ field may not necessarily represent a single Unicode character. To
ensure that multi-byte characters are not broken up, the application can
concatenate multiple data tokens to a single buffer before attempting to do
further processing on the result.

To make processing easier, an application may want to combine all the tokens
into a single buffer. This can be easily implemented as follows:

```c
SomeString attrval;
while(..) {
  yxml_ret_t r = yxml_parse(x, ch);
  switch(r) {
  case YXML_ATTRSTART:
    somestring_initialize(attrval);
    break;
  case YXML_ATTRVAL:
    somestring_append(attrval, x->data);
    break;
  case YXML_ATTREND:
    /* Now we have a full attribute. Its name is in x->attr, and its value is
     * in the string 'attrval'. */
    somestring_reset(attrval);
    break;
  }
}
```

The `SomeString` type and `somestring_` functions are stubs for any string
handling library of your choosing. When using Glib, for example, one could use
the [GString](https://developer.gnome.org/glib/stable/glib-Strings.html)
type and the `g_string_new()`, `g_string_append()` and `g_string_free()`
functions. For a more lighter-weight string library there is also
[kstring.h in klib](https://github.com/attractivechaos/klib), but the
functionality required in the above example can easily be implemented in a few
lines of pure C, too.

When buffering data into an ever-growing string, as done in the previous
example, one should be careful to protect against memory exhaustion. This can
be done trivially by limiting the size of the total XML document or the maximum
length of the buffer. If you want to extract information from an XML document
that might not fit into memory, but you know that the information you care
about is limited in size and is only stored in specific attributes or elements,
you can choose to ignore data you don't care about. For example, if you only
want to extract the "Size" attribute and you know that its value is never
larger than 63 bytes, you can limit your code to read only that value and store
it into a small pre-allocated buffer:

```c
char sizebuf[64], *sizecur = NULL, *tmp;
while(..) {
  yxml_ret_t r = yxml_parse(x, ch);
  switch(r) {
  case YXML_ATTRSTART:
    if(strcmp(x->attr, "Size") == 0)
      sizecur = sizebuf;
    break;
  case YXML_ATTRVAL:
    if(!sizecur) /* Are we in the "Size" attribute? */
      break;
    /* Append x->data to sizecur while there is space */
    tmp = x->data;
    while(*tmp && sizecur < sizebuf+sizeof(sizebuf))
      *(sizecur++) = *(tmp++);
    if(sizecur == sizebuf+sizeof(sizebuf))
      exit(1); /* Too long attribute value, handle error */
    *sizecur = 0;
    break;
  case YXML_ATTREND:
    if(sizecur) {
      /* Now we have the value of the "Size" attribute in sizebuf */
      sizecur = NULL;
    }
    break;
  }
}
```

## Finalization

```c
yxml_t *x; /* An initialized state */
yxml_ret_t r = yxml_eof(x);
if(r < 0)
  exit(1); /* Handle error */
else
  /* No errors in the XML document */
```

Because `yxml_parse()` does not know when the end of the XML document has been
reached, it is unable to detect certain errors in the document. This is why,
after successfully parsing a complete document with `yxml_parse()`, the
application should call `yxml_eof()` to perform some extra checks.

`yxml_eof()` will return `YXML_OK` if the parsed XML document is well-formed,
`YXML_EEOF` otherwise. The following errors are not detected by
`yxml_parse()` but will result in an error on `yxml_eof()`:

- The XML document did not contain a root element (e.g. an empty file).
- The XML root element has not been closed (e.g. "`<a> ..`").
- The XML document ended in the middle of a comment or PI (e.g.
  "`<a/><!-- ..`").

## Utility functions

```c
size_t yxml_symlen(yxml_t *, const char *);
```

`yxml_symlen()` returns the length of the element name (`x->elem`), attribute
name (`x->attr`), or PI name (`x->pi`). When used correctly, it gives the same
result as `strlen()`, except without having to scan through the string. This
function should **ONLY** be used directly after the `YXML_ELEMSTART`,
`YXML_ATTRSTART` or `YXML_PISTART` (respectively) tokens have been returned by
`yxml_parse()`, calling this function at any other time may not give the
correct results. This function should **NOT** be used on strings other than
`x->elem`, `x->attr` or `x->pi`.
