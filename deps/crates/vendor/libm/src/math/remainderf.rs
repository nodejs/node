#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn remainderf(x: f32, y: f32) -> f32 {
    let (result, _) = super::remquof(x, y);
    result
}
