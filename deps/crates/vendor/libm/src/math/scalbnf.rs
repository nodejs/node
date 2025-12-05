#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn scalbnf(x: f32, n: i32) -> f32 {
    super::generic::scalbn(x, n)
}
