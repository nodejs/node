#[diplomat::bridge]
mod ffi {
    pub fn mut_str(st : &mut DiplomatStr) {}
}