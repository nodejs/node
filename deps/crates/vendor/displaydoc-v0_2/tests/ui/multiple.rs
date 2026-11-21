use displaydoc::Display;

/// this type is pretty swell
#[derive(Display)]
struct FakeType;

static_assertions::assert_impl_all!(FakeType: core::fmt::Display);

/// this type is pretty swell2
#[derive(Display)]
struct FakeType2;

static_assertions::assert_impl_all!(FakeType2: core::fmt::Display);

fn main() {}