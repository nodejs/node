//! Header: `stdlib.h`

use super::*;
use crate::prelude::*;

// Exit status constants
pub const EXIT_SUCCESS: c_int = 0;
pub const EXIT_FAILURE: c_int = 1;

// Maximum values
pub const RAND_MAX: c_int = 32767;

extern "C" {
    // Memory management
    pub fn malloc(size: size_t) -> *mut c_void;
    pub fn calloc(nmemb: size_t, size: size_t) -> *mut c_void;
    pub fn realloc(ptr: *mut c_void, size: size_t) -> *mut c_void;
    pub fn free(ptr: *mut c_void);

    // Process control
    pub fn abort() -> !;
    pub fn exit(status: c_int) -> !;
    pub fn atexit(function: extern "C" fn()) -> c_int;

    // Environment
    pub fn getenv(name: *const c_char) -> *mut c_char;
    pub fn setenv(name: *const c_char, value: *const c_char, overwrite: c_int) -> c_int;
    pub fn unsetenv(name: *const c_char) -> c_int;

    // String/number conversion
    pub fn atoi(nptr: *const c_char) -> c_int;
    pub fn atol(nptr: *const c_char) -> c_long;
    pub fn atoll(nptr: *const c_char) -> c_longlong;
    pub fn strtol(nptr: *const c_char, endptr: *mut *mut c_char, base: c_int) -> c_long;
    pub fn strtoul(nptr: *const c_char, endptr: *mut *mut c_char, base: c_int) -> c_ulong;
    pub fn strtoll(nptr: *const c_char, endptr: *mut *mut c_char, base: c_int) -> c_longlong;
    pub fn strtoull(nptr: *const c_char, endptr: *mut *mut c_char, base: c_int) -> c_ulonglong;
    pub fn strtod(nptr: *const c_char, endptr: *mut *mut c_char) -> c_double;
    pub fn strtof(nptr: *const c_char, endptr: *mut *mut c_char) -> c_float;

    // Random numbers
    pub fn rand() -> c_int;
    pub fn srand(seed: c_uint);

    // Searching and sorting
    pub fn qsort(
        base: *mut c_void,
        nmemb: size_t,
        size: size_t,
        compar: extern "C" fn(*const c_void, *const c_void) -> c_int,
    );
    pub fn bsearch(
        key: *const c_void,
        base: *const c_void,
        nmemb: size_t,
        size: size_t,
        compar: extern "C" fn(*const c_void, *const c_void) -> c_int,
    ) -> *mut c_void;

    // Integer arithmetic
    pub fn abs(j: c_int) -> c_int;
    pub fn labs(j: c_long) -> c_long;
    pub fn llabs(j: c_longlong) -> c_longlong;
    pub fn div(numer: c_int, denom: c_int) -> div_t;
    pub fn ldiv(numer: c_long, denom: c_long) -> ldiv_t;
    pub fn lldiv(numer: c_longlong, denom: c_longlong) -> lldiv_t;
}
