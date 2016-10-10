marked-man(1) -- markdown to roff
=================================

SYNOPSIS
--------

```
marked-man README.md > doc/marked-man.1
```

See [marked README](https://github.com/chjj/marked) for documentation about how to use marked.

Note that `marked-man --format=html` is the same as `marked`.


DESCRIPTION
-----------

`marked-man` wraps `marked` to extend it with groff output support.


OPTIONS
-------

`marked-man` adds some options to `marked` existing options:

* format
  Sets the output format. Outputs html if different from `roff`.
  Defaults to `roff`.

* name
  The name shown in the manpage header, if it isn't given in the ronn header like in this README.
  Defaults to empty string.

* section
  The section shown in the manpage header, if it isn't given in the ronn header like in this README.
  Defaults to empty string.

* version
  The version shown in the manpage header.
  Defaults to empty string.

* manual
  The MANUAL string shown in the manpage header.
  Defaults to empty string.

* date
  The date shown in the manpage header.
  Defaults to now, must be acceptable by new Date(string).

`marked-man` invokes `marked --gfm --sanitize`.
The --breaks option can be helpful to match default `ronn` behavior.


SEE ALSO
--------

[Ryan Tomayko](https://github.com/rtomayko/ronn)


REPORTING BUGS
--------------

See [marked-man repository](https://github.com/kapouer/marked-man).
