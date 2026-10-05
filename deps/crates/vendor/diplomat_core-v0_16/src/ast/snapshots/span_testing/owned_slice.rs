#[diplomat::bridge]
mod ffi {
    pub fn test(owned : Box<[Type]>) {}
}