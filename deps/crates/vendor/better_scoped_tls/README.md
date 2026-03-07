# better_scoped_tls

This crate provides an opinionated version of [scoped-tls](https://docs.rs/scoped-tls/1.0.0/scoped_tls/index.html).

Scoped thread local variables created by this crate will panic with a good message on usage without `.set`, like

```
You should perform this operation in the closure passed to `set` of better_scoped_tls::tests::TESTTLS
```

Syntax is exactly same to the original scoped-tls.
