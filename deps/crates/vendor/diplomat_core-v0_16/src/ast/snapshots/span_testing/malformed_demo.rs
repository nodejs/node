#[diplomat::bridge]
mod ffi {
    #[diplomat::demo(123)]
    pub fn malformed_cfg() {}
}