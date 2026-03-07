use crate::prelude::*;

extern "C" {
    #[cfg(any(target_os = "linux", target_os = "l4re"))]
    pub fn pthread_getaffinity_np(
        thread: crate::pthread_t,
        cpusetsize: size_t,
        cpuset: *mut crate::cpu_set_t,
    ) -> c_int;

    pub fn pthread_getattr_np(native: crate::pthread_t, attr: *mut crate::pthread_attr_t) -> c_int;

    #[cfg(target_os = "linux")]
    pub fn pthread_getname_np(thread: crate::pthread_t, name: *mut c_char, len: size_t) -> c_int;

    #[cfg(any(target_os = "linux", target_os = "l4re"))]
    pub fn pthread_setaffinity_np(
        thread: crate::pthread_t,
        cpusetsize: size_t,
        cpuset: *const crate::cpu_set_t,
    ) -> c_int;

    #[cfg(any(target_os = "android", target_os = "linux"))]
    pub fn pthread_setname_np(thread: crate::pthread_t, name: *const c_char) -> c_int;
}
