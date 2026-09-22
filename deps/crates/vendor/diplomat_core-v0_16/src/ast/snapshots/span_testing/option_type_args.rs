#[diplomat::bridge]
mod ffi {
    pub fn test(opt: Option<32>) {}
}