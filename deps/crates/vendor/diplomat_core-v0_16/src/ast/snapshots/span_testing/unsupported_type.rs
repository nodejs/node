#[diplomat::bridge]
mod ffi {
    pub fn unsupported_type(a : fn()) {}
}