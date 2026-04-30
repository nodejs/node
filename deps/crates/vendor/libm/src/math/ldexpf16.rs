#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn ldexpf16(x: f16, n: i32) -> f16 {
    super::scalbnf16(x, n)
}
