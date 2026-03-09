use is_macro::Is;

#[derive(Debug, Is)]
pub enum Enum {
    A,
    B(String),
    C(bool),
}
