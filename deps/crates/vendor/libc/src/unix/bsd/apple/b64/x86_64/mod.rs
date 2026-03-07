use crate::prelude::*;

pub type boolean_t = c_uint;

s! {
    pub struct malloc_introspection_t {
        _private: [crate::uintptr_t; 16], // FIXME(macos): keeping private for now
    }

    // FIXME(1.0): This should not implement `PartialEq`
    #[allow(unpredictable_function_pointer_comparisons)]
    pub struct malloc_zone_t {
        _reserved1: Padding<*mut c_void>,
        _reserved2: Padding<*mut c_void>,
        pub size:
            Option<unsafe extern "C" fn(zone: *mut malloc_zone_t, ptr: *const c_void) -> size_t>,
        pub malloc:
            Option<unsafe extern "C" fn(zone: *mut malloc_zone_t, size: size_t) -> *mut c_void>,
        pub calloc: Option<
            unsafe extern "C" fn(
                zone: *mut malloc_zone_t,
                num_items: size_t,
                size: size_t,
            ) -> *mut c_void,
        >,
        pub valloc:
            Option<unsafe extern "C" fn(zone: *mut malloc_zone_t, size: size_t) -> *mut c_void>,
        pub free: Option<unsafe extern "C" fn(zone: *mut malloc_zone_t, ptr: *mut c_void)>,
        pub realloc: Option<
            unsafe extern "C" fn(
                zone: *mut malloc_zone_t,
                ptr: *mut c_void,
                size: size_t,
            ) -> *mut c_void,
        >,
        pub destroy: Option<unsafe extern "C" fn(zone: *mut malloc_zone_t)>,
        pub zone_name: *const c_char,
        pub batch_malloc: Option<
            unsafe extern "C" fn(
                zone: *mut malloc_zone_t,
                size: size_t,
                results: *mut *mut c_void,
                num_requested: c_uint,
            ) -> c_uint,
        >,
        pub batch_free: Option<
            unsafe extern "C" fn(
                zone: *mut malloc_zone_t,
                to_be_freed: *mut *mut c_void,
                num_to_be_freed: c_uint,
            ),
        >,
        pub introspect: *mut malloc_introspection_t,
        pub version: c_uint,
        pub memalign: Option<
            unsafe extern "C" fn(
                zone: *mut malloc_zone_t,
                alignment: size_t,
                size: size_t,
            ) -> *mut c_void,
        >,
        pub free_definite_size:
            Option<unsafe extern "C" fn(zone: *mut malloc_zone_t, ptr: *mut c_void, size: size_t)>,
        pub pressure_relief:
            Option<unsafe extern "C" fn(zone: *mut malloc_zone_t, goal: size_t) -> size_t>,
        pub claimed_address: Option<
            unsafe extern "C" fn(zone: *mut malloc_zone_t, ptr: *mut c_void) -> crate::boolean_t,
        >,
    }
}

s_no_extra_traits! {
    #[repr(align(16))]
    pub struct max_align_t {
        priv_: [f64; 2],
    }
}
