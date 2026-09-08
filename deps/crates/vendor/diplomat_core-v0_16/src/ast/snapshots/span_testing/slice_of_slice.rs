#[diplomat::bridge]
mod ffi {
    pub fn test(sl : &[&str]) {}
}