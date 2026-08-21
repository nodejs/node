# Reproducing Security Bugs

V8 offers many different configurations and modes.
Not all bugs filed are actually security bugs.
This document describes some of the configurations that can help with identifying whether a bug is a vulnerability or just a regular correctness bug.

## Regular security bugs

Reproductions (POCs) should work on `d8`.
To help identify whether a bug is a valid vulnerability or a regular correctness bug, various flags are used to filter out configurations that are not relevant for security testing.

The recommended way to verify if a bug is a security vulnerability is to run the reproduction with `--run-as-security-poc` in addition to any provided flags.
If the crash still occurs with this flag enabled, it is likely a valid security vulnerability (**Type=Vulnerability**).

### Fine-grained control over configurations

`--run-as-security-poc` implies the following flags, which can also be manually set to narrow down the problem space:

- `--disallow-unsafe-flags`: By default, `d8` runs with a variety of flags that are considered unsafe in the context of security testing.
This flag disables them.
**Setting this flag is the bare minimum requirement for a bug to be considered a security bug.**
- `--disallow-developer-only-features`: Prevents the usage of features that are only available to developers.
- `--no-experimental`: Prevents the usage of experimental features that are not yet ready for shipping.
- `--fuzzing`: Enables internal fuzzing support. It is often used to expose edge cases.

If a POC stops reproducing when any of the above flags are set, it is likely not a security bug (**Type=Bug**, **Security_Impact-None**).
