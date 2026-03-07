use seq_macro::seq;

seq!(N in 0..1 {
    fn main() {
        let _ = Missing~N;
    }
});
