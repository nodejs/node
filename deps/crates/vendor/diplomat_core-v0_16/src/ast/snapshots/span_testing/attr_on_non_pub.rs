#[diplomat::bridge]
mod ffi {
    pub struct Test {}

    impl Test {
        #[diplomat::attr(*, disable)]
        fn hidden_fn() {}
    }
}