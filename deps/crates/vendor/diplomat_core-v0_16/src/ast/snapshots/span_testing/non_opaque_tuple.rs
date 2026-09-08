#[diplomat::bridge]
mod ffi {
    pub struct Tuple(i32, i32);
}