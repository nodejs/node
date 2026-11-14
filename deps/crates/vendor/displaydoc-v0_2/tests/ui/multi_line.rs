use displaydoc::Display;

/// this type is pretty swell
#[derive(Display)]
enum TestType {
    /// This one is okay
    Variant1,

    /// Multi
    /// line
    /// doc
    /// is
    /// pretty
    /// swell
    Variant2,
}

static_assertions::assert_impl_all!(TestType: core::fmt::Display);

fn main() {}
