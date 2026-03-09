pub fn select_unpredictable<T>(condition: bool, true_val: T, false_val: T) -> T {
    if condition {
        true_val
    } else {
        false_val
    }
}
