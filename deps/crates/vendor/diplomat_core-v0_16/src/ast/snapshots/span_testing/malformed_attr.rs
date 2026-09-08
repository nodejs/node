#[diplomat::bridge]
mod ffi {
    #[diplomat::attr(**)]
    pub fn malformed_attr() {}
}