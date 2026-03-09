use roaring::RoaringBitmap;

pub fn id_start_bitmap() -> RoaringBitmap {
    let mut bitmap = RoaringBitmap::new();
    for ch in '\0'..=char::MAX {
        if unicode_id_start::is_id_start(ch) {
            bitmap.insert(ch as u32);
        }
    }
    bitmap
}

pub fn id_continue_bitmap() -> RoaringBitmap {
    let mut bitmap = RoaringBitmap::new();
    for ch in '\0'..=char::MAX {
        if unicode_id_start::is_id_continue(ch) {
            bitmap.insert(ch as u32);
        }
    }
    bitmap
}
