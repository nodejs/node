//! Header: `dlfcn.h`
//!
//! Dynamic linking functions and constants from Hexagon toolchain.

use crate::prelude::*;

// Values for dlopen `mode`
pub const RTLD_LAZY: c_int = 1;
pub const RTLD_NOW: c_int = 2;
pub const RTLD_GLOBAL: c_int = 0x100;
pub const RTLD_LOCAL: c_int = 0x200;

// Compatibility constant
pub const DL_LAZY: c_int = RTLD_LAZY;

// Special handles
pub const RTLD_NEXT: *mut c_void = -1isize as *mut c_void;
pub const RTLD_DEFAULT: *mut c_void = -2isize as *mut c_void;
pub const RTLD_SELF: *mut c_void = -3isize as *mut c_void;

extern "C" {
    pub fn dlopen(filename: *const c_char, flag: c_int) -> *mut c_void;
    pub fn dlclose(handle: *mut c_void) -> c_int;
    pub fn dlsym(handle: *mut c_void, symbol: *const c_char) -> *mut c_void;
    pub fn dlerror() -> *mut c_char;
}
