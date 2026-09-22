#[diplomat::bridge]
mod ffi {
    pub fn ns_lifetime(t : impl Fn() + 'a){}
}