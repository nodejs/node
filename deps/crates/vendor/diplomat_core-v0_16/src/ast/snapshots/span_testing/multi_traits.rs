#[diplomat::bridge]
mod ffi {
    pub fn multi_traits(t : impl Fn() + FnMut()) {}
}