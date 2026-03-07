use is_macro::Is;

#[derive(Debug, PartialEq, Is)]
pub enum Enum {
    A(u32),
    B(Vec<u32>),
}

#[test]
fn test() {
    let mut e = Enum::A(0);
    *e.as_mut_a().unwrap() += 1;
    assert_eq!(e, Enum::A(1));

    let mut e = Enum::B(vec![]);
    e.as_mut_b().unwrap().push(1);
    assert_eq!(e, Enum::B(vec![1]));
}
