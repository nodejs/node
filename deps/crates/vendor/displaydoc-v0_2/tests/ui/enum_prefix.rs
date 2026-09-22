use displaydoc::Display;

/// this type is pretty swell
#[derive(Display)]
#[prefix_enum_doc_attributes]
enum TestType {
    /// this variant is too
    Variant1,

    /// this variant is two
    Variant2,
}

static_assertions::assert_impl_all!(TestType: core::fmt::Display);

fn main() {}