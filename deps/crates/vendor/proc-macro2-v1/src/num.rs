// TODO: use NonZero<char> in Rust 1.89+
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct NonZeroChar(char);

impl NonZeroChar {
    pub fn new(ch: char) -> Option<Self> {
        if ch == '\0' {
            None
        } else {
            Some(NonZeroChar(ch))
        }
    }

    pub fn get(self) -> char {
        self.0
    }
}
