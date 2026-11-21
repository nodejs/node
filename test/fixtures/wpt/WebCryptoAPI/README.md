Tests for the Web Cryptography API are grouped by specification:

- Tests for [Web Cryptography API Level 2](https://w3c.github.io/webcrypto/)
  are in this directory's existing operation subdirectories and root files.
- Tests for [Modern Algorithms in the Web Cryptography API](https://wicg.github.io/webcrypto-modern-algos/)
  are in `modern-algos/`, including `supports()`, `getPublicKey()`, and key format extensions.
- Tests for [Secure Curves in the Web Cryptography API](https://wicg.github.io/webcrypto-secure-curves/)
  are in `secure-curves/`. Ed25519 and X25519 are part of the core specification;
  Ed448 and X448 are covered by the secure curves specification.

Each specification directory has a `META.yml` linking to its specification.
Helpers used across specifications remain in the shared operation directories
or `util/`; helpers specific to an extension are alongside that extension's tests.

Use `.tentative` only for assertions not yet required by a specification or that
contradict a specification, as described in the [file naming guidelines](../docs/writing-tests/file-names.md).
An algorithm being defined in an extension specification does not by itself make
its tests tentative.
