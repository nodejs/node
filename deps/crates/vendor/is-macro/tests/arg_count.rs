use is_macro::Is;

#[derive(Debug, Is)]
pub enum Enum {
    A(),
    B(usize, usize),
    C(String),
    D(&'static str, &'static mut u32),
}
