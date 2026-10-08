#[diplomat::bridge]
mod ffi {
    pub fn takes_tuple(t : (i32, i32)) {}
}