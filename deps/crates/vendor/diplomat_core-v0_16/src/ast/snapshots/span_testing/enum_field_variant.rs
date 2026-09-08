#[diplomat::bridge]
mod ffi {
    enum EnumWithField {
        Field(String)
    }
}