#[diplomat::bridge]
mod ffi {
    pub fn unsupported_bound(b : impl use<'a, T> + ABC) {}
}