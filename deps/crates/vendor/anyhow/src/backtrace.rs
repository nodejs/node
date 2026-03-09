#[cfg(feature = "std")]
pub(crate) use std::backtrace::Backtrace;

#[cfg(not(feature = "std"))]
pub(crate) enum Backtrace {}

#[cfg(feature = "std")]
macro_rules! backtrace {
    () => {
        Some(std::backtrace::Backtrace::capture())
    };
}

#[cfg(not(feature = "std"))]
macro_rules! backtrace {
    () => {
        None
    };
}

#[cfg(error_generic_member_access)]
macro_rules! backtrace_if_absent {
    ($err:expr) => {
        match $crate::nightly::request_ref_backtrace($err as &dyn core::error::Error) {
            Some(_) => None,
            None => backtrace!(),
        }
    };
}

#[cfg(all(not(error_generic_member_access), feature = "std"))]
macro_rules! backtrace_if_absent {
    ($err:expr) => {
        backtrace!()
    };
}

#[cfg(all(not(anyhow_no_core_error), not(feature = "std")))]
macro_rules! backtrace_if_absent {
    ($err:expr) => {
        None
    };
}

fn _assert_send_sync() {
    fn assert<T: Send + Sync>() {}
    assert::<Backtrace>();
}
