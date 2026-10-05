#[diplomat::bridge]
mod ffi {
    pub fn unsupported_fn_type(f : impl Fn<>) {}
}