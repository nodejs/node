#[diplomat::bridge]
mod ffi {
    pub fn lifetime_callback_params(c : impl for<'a> Fn(&'a i32)) {}
}