For build instructions, please refer to:

https://code.google.com/p/v8/wiki/BuildingWithGYP

TL;DR version on *nix:
$ make dependencies        # Only needed once.
$ make ia32.release -j8
$ make ia32.release.check  # Optionally: run tests.

