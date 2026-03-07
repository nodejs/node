use is_macro::Is;

#[derive(Debug, Is)]
pub enum Enum {
    A,
    B(&'static str),
    C(&'static mut u32),
}
