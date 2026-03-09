//! Source from libpthread <https://github.com/apple-oss-distributions/libpthread/tree/main>

/// Directory: `pthread/`
///
/// Note that this module has a trailing underscore to avoid conflicting with its child `pthread`
/// module.
///
/// <https://github.com/apple-oss-distributions/libpthread/tree/main/include/pthread>
pub(crate) mod pthread_ {
    pub(crate) mod introspection;
    pub(crate) mod pthread;
    pub(crate) mod pthread_impl;
    pub(crate) mod pthread_spis;
    pub(crate) mod qos;
    pub(crate) mod sched;
    pub(crate) mod spawn;
    pub(crate) mod stack_np;
}

pub(crate) mod sys;
