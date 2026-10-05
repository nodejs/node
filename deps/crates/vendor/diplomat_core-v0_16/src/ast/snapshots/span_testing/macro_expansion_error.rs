#[diplomat::bridge]
mod ffi {
    #[diplomat::macro_rules]
    macro_rules! test {
        ($arg:ident) => {
            $123
        };
    }

    test!(test);
}