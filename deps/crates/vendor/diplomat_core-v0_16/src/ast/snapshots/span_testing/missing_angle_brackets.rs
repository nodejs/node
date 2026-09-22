#[diplomat::bridge]
mod ffi {
    pub fn test(some_box : Box) {}
}