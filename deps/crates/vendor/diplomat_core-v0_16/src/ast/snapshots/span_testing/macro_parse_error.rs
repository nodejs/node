#[diplomat::bridge]
mod ffi {
    #[diplomat::macro_rules]
    macro_rules! test {
        ($t:ident) => {};
    }

    test!(123);
}