#[diplomat::bridge]
mod ffi {
    pub struct Test {}
    impl Test {
        pub fn with_generics<T> () {}
    }
}