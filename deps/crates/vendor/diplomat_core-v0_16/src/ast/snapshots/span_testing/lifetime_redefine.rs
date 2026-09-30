#[diplomat::bridge]
mod ffi {
    pub fn lifetime_redefine<'a, 'a>() {}
}