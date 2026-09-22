#[diplomat::bridge]
mod ffi {
    pub fn test(t : impl TraitWithGenerics<T>) {}
}