#[diplomat::bridge]
mod ffi {
    #[diplomat::cfg(;)]
    pub fn malformed_cfg() {}
}