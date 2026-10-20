#[diplomat::bridge]
mod ffi {
    #[diplomat::out]
    #[diplomat::opaque]
    pub struct SomeStruct;
}