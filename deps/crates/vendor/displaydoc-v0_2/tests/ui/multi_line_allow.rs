use displaydoc::Display;

/// this type is pretty swell
#[derive(Display)]
#[ignore_extra_doc_attributes]
enum TestType {
    /// This one is okay
    Variant1,

    /// Multi
    /// line
    /// doc.
    Variant2,

    /// Multi
    /// line
    /// doc
    ///
    /// with
    /// line
    /// breaks
    Variant3,
}

static_assertions::assert_impl_all!(TestType: core::fmt::Display);

fn main() {}
