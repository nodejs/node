use displaydoc::Display;

#[derive(Display)]
/// Multi
/// line
/// doc
/// with
/// line
/// break
///
/// is
/// pretty
/// not
/// swell
/// 😞👊
struct TestType;

static_assertions::assert_impl_all!(TestType: core::fmt::Display);

fn main() {}
