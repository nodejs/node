//! Directory: `sys/`
//!
//! <https://github.com/apple-oss-distributions/libpthread/tree/main/include/sys/_pthread>

/// Directory: `sys/_pthread/`
///
/// <https://github.com/apple-oss-distributions/libpthread/tree/main/include/sys>
pub(crate) mod _pthread {
    // We don't have the `_pthread_attr_t` and similar modules to match `_pthread_attr_t.h`,
    // everything is defined in `_pthread_types`.
    pub(crate) mod _pthread_types;
}

pub(crate) mod qos;
