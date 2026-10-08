use displaydoc::Display;

#[derive(Display)]
/**
 * block
 */
/**
 *
 */
struct TestType;

static_assertions::assert_impl_all!(TestType: core::fmt::Display);

fn main() {}
