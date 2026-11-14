use displaydoc::Display;

/// this type is pretty swell
struct FakeType;

static_assertions::assert_impl_all!(FakeType: core::fmt::Display);

fn main() {}
