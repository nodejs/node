#[diplomat::bridge]
mod ffi {
    #[diplomat::rust_link(*)]
    pub fn malformed_rust_link() {}
}