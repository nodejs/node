macro_rules! check {
    ($f:tt) => {
        assert_eq!(to_chars($f), stringify!($f));
    };
    (-$f:tt) => {
        assert_eq!(to_chars(-$f), concat!("-", stringify!($f)));
    };
}
