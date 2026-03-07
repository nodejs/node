//! This module contains the future directory structure. If possible, new definitions should
//! get added here.
//!
//! Eventually everything should be moved over, and we will move this directory to the top
//! level in `src`.
//!
//! # Basic structure
//!
//! Each child module here represents a library or group of libraries that we are binding. Each of
//! these has several submodules, representing either a directory or a header file in that library.
//!
//! `#include`s turn into `pub use ...*;` statements. Then at the root level (here), we choose
//! which top-level headers we want to reexport the definitions for.
//!
//! All modules are only crate-public since we don't reexport this structure.

mod common;

/* The `pub(crate) use ...` statements are commented while the modules are empty */

// Platform libraries and combined platform+libc
//
// Not supported or not needed:
// * amdhsa
// * cuda
// * lynxos178
// * managarm
// * motor
// * none
// * psp
// * psx
// * uefi
// * unknown
// * vexos
// * zkvm
//
// Combined to target_vendor = "apple"
// * ios
// * macos
// * tvos
// * visionos
// * watchos
cfg_if! {
    if #[cfg(target_os = "aix")] {
        mod aix;
        pub(crate) use aix::*;
    } else if #[cfg(target_os = "android")] {
        mod bionic_libc;
        pub(crate) use bionic_libc::*;
    } else if #[cfg(target_vendor = "apple")] {
        mod apple;
        pub(crate) use apple::*;
    } else if #[cfg(target_os = "cygwin")] {
        mod cygwin;
        pub(crate) use cygwin::*;
    } else if #[cfg(target_os = "dragonfly")] {
        mod dragonfly;
        pub(crate) use dragonfly::*;
    } else if #[cfg(target_os = "emscripten")] {
        mod emscripten;
        pub use emscripten::sched::*;
        pub(crate) use emscripten::*;
    } else if #[cfg(target_os = "espidf")] {
        mod espidf;
        // pub(crate) use espidf::*;
    } else if #[cfg(target_os = "freebsd")] {
        mod freebsd;
        pub(crate) use freebsd::*;
    } else if #[cfg(target_os = "fuchsia")] {
        mod fuchsia;
        pub(crate) use fuchsia::*;
    } else if #[cfg(target_os = "haiku")] {
        mod haiku;
        pub(crate) use haiku::*;
    } else if #[cfg(target_os = "hermit")] {
        mod hermit_abi;
        // pub(crate) use hermit_abi::*;
    } else if #[cfg(target_os = "horizon")] {
        mod horizon;
        // pub(crate) use horizon::*;
    } else if #[cfg(target_os = "hurd")] {
        mod hurd;
        // pub(crate) use hurd::*;
    } else if #[cfg(target_os = "illumos")] {
        mod illumos;
        pub(crate) use illumos::*;
    } else if #[cfg(target_os = "l4re")] {
        mod l4re;
        // pub(crate) use l4re::*;
    } else if #[cfg(target_os = "linux")] {
        mod linux_uapi;
        pub(crate) use linux_uapi::*;
    } else if #[cfg(target_os = "netbsd")] {
        mod netbsd;
        pub(crate) use netbsd::*;
    } else if #[cfg(target_os = "nto")] {
        mod nto;
        pub(crate) use nto::*;
    } else if #[cfg(target_os = "nuttx")] {
        mod nuttx;
        pub(crate) use nuttx::*;
    } else if #[cfg(target_os = "openbsd")] {
        mod openbsd;
        pub(crate) use openbsd::*;
    } else if #[cfg(target_os = "qurt")] {
        pub mod qurt;
        pub use qurt::*;
    } else if #[cfg(target_os = "redox")] {
        mod redox;
        // pub(crate) use redox::*;
    } else if #[cfg(target_os = "rtems")] {
        mod rtems;
        // pub(crate) use rtems::*;
    } else if #[cfg(target_os = "solaris")] {
        mod solaris;
        pub(crate) use solaris::*;
    } else if #[cfg(target_os = "solid_asp3")] {
        mod solid;
        // pub(crate) use solid::*;
    } else if #[cfg(target_os = "teeos")] {
        mod teeos;
        // pub(crate) use teeos::*;
    } else if #[cfg(target_os = "trusty")] {
        mod trusty;
        // pub(crate) use trusty::*;
    } else if #[cfg(target_os = "vita")] {
        mod vita;
        // pub(crate) use vita::*;
    } else if #[cfg(target_os = "vxworks")] {
        mod vxworks;
        pub(crate) use vxworks::*;
    } else if #[cfg(target_os = "wasi")] {
        mod wasi;
        // pub(crate) use wasi::*;
    } else if #[cfg(target_os = "windows")] {
        mod ucrt;
        // pub(crate) use ucrt::*;
    } else if #[cfg(target_os = "xous")] {
        mod xous;
        // pub(crate) use xous::*;
    }
}

// Multi-platform libc
cfg_if! {
    // FIXME(vxworks): vxworks sets `target_env = "gnu"` but maybe shouldn't.
    if #[cfg(all(
        target_family = "unix",
        target_env = "gnu",
        not(target_os = "vxworks")
    ))] {
        mod glibc;
        pub(crate) use glibc::*;
    } else if #[cfg(any(target_env = "musl", target_env = "ohos"))] {
        // OhOS also uses the musl libc
        mod musl;
        pub use musl::sched::*;
        pub(crate) use musl::*;
    } else if #[cfg(target_env = "newlib")] {
        mod newlib;
        pub(crate) use newlib::*;
    } else if #[cfg(target_env = "relibc")] {
        mod relibc;
        pub(crate) use relibc::*;
    } else if #[cfg(target_env = "sgx")] {
        mod sgx;
        // pub(crate) use sgx::*;
    } else if #[cfg(target_env = "uclibc")] {
        mod uclibc;
        pub(crate) use uclibc::*;
    }
}

// Per-OS headers we export
cfg_if! {
    if #[cfg(target_os = "android")] {
        pub use sys::socket::*;
    } else if #[cfg(target_os = "linux")] {
        pub use linux::can::bcm::*;
        pub use linux::can::error::*;
        pub use linux::can::j1939::*;
        pub use linux::can::raw::*;
        pub use linux::can::*;
        pub use linux::keyctl::*;
        pub use linux::membarrier::*;
        pub use linux::netlink::*;
        #[cfg(target_env = "gnu")]
        pub use net::route::*;
    } else if #[cfg(target_vendor = "apple")] {
        pub use pthread::*;
        pub use pthread_::introspection::*;
        pub use pthread_::pthread_spis::*;
        pub use pthread_::spawn::*;
        pub use pthread_::stack_np::*;
        pub use signal::*;
    } else if #[cfg(target_os = "netbsd")] {
        pub use net::if_::*;
        pub use sys::ipc::*;
        pub use sys::statvfs::*;
        pub use sys::time::*;
        pub use sys::timex::*;
        pub use sys::types::*;
        pub use utmp_::*;
        pub use utmpx_::*;
    } else if #[cfg(target_os = "openbsd")] {
        pub use sys::ipc::*;
    } else if #[cfg(target_os = "nto")] {
        pub use net::bpf::*;
        pub use net::if_::*;
    }
}

// Per-env headers we export
cfg_if! {
    if #[cfg(any(target_env = "musl", target_env = "ohos"))] {
        pub use sys::socket::*;
    }
}

// Per-family headers we export
cfg_if! {
    if #[cfg(all(target_family = "unix", not(target_os = "qurt")))] {
        // FIXME(pthread): eventually all platforms should use this module
        #[cfg(any(
            target_os = "android",
            target_os = "emscripten",
            target_os = "l4re",
            target_os = "linux"
        ))]
        pub use pthread::*;
        pub use unistd::*;
    }
}
