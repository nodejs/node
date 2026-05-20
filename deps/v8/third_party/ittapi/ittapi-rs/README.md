# ittapi-rs
This package creates Rust bindings for ittapi. Ittapi is used for instrumentation and tracking and just-in-time profiling.

Note the building of this package is currently only supported (tested) on a couple of Linux platforms (recent Ubuntu and Fedora builds) but support for other platforms is intended and contributions are welcomed.

# Build the package
cargo build

** Note This package uses bindgen which in turn requires recent versions of cmake and llvm to be installed. For Fedora, "yum install llvm-devel" was needed to bring in llvm-config.

** Also note building this package requires rust nightly.

# Publish the package to crates.io
cargo publish