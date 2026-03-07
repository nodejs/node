use is_macro::Is;

#[derive(Debug, Is)]
pub enum Enum<T> {
    A,
    B(T),
    C(Option<T>),
}
