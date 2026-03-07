use swc_config::merge::Merge;

#[derive(Merge)]
struct Fields {
    a: Option<()>,
}

#[test]
fn test_fields() {
    let mut fields = Fields { a: None };
    fields.merge(Fields { a: Some(()) });

    assert_eq!(fields.a, Some(()));
}

#[derive(Merge)]
struct Tuple(Option<()>);
#[test]
fn test_tuple() {
    let mut tuple = Tuple(None);
    tuple.merge(Tuple(Some(())));

    assert_eq!(tuple.0, Some(()));
}
