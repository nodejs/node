#[diplomat::bridge]
mod ffi {
    fn hidden() -> Result<T> {}
    pub fn return_result() -> Result<T> {}
}