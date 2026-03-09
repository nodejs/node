# par-core

A wrapper for various parallelization library for Rust.
This crate currently supports

- [`chili`](https://github.com/dragostis/chili)
- [`rayon`](https://github.com/rayon-rs/rayon)
- Disable parallelization.

# Usage

If you are developing a library, you should not force the parallelization library, and let the users choose the parallelization library.

## Final application

If you are developing a final application, you can use cargo feature to select the parallelization library.

### `chili`

```toml
[dependencies]
par-core = { version = "1.0.3", features = ["chili"] }
```

### `rayon`

```toml
[dependencies]
par-core = { version = "1.0.3", features = ["rayon"] }
```

### Disable parallelization

```toml
[dependencies]
par-core = { version = "1.0.3", default-features = false }
```

## Library developers

If you are developing a library, you can simply depend on `par-core` without any features.
**Note**: To prevent a small mistake of end-user making the appplication slower, `par-core` emits a error message using a default feature.
So if you are a library developer, you should specify `default-features = false`.

```toml
[dependencies]
par-core = { version = "1.0.3", default-features = false }
```

# License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE) file for details.
