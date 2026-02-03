#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn scalbnf16(x: f16, n: i32) -> f16 {
    super::generic::scalbn(x, n)
}
