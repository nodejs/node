use std::{iter, ptr};

/// Modifiers vector in-place.
pub trait MoveMap<T>: Sized {
    /// Map in place.
    fn move_map<F>(self, mut f: F) -> Self
    where
        F: FnMut(T) -> T,
    {
        self.move_flat_map(|e| iter::once(f(e)))
    }

    /// This will be very slow if you try to extend vector using this method.
    ///
    /// This method exists to drop useless nodes. You can return Option to do
    /// such shortening.
    fn move_flat_map<F, I>(self, f: F) -> Self
    where
        F: FnMut(T) -> I,
        I: IntoIterator<Item = T>;
}

impl<T> MoveMap<T> for Vec<T> {
    /// This reduces binary size.
    fn move_map<F>(mut self, mut f: F) -> Self
    where
        F: FnMut(T) -> T,
    {
        unsafe {
            let old_len = self.len();
            self.set_len(0); // make sure we just leak elements in case of panic

            for index in 0..old_len {
                let item_ptr = self.as_mut_ptr().add(index);
                // move the item out of the vector and map it
                let item = ptr::read(item_ptr);
                let item = f(item);

                ptr::write(item_ptr, item);
            }

            // restore the original length
            self.set_len(old_len);
        }

        self
    }

    fn move_flat_map<F, I>(mut self, mut f: F) -> Self
    where
        F: FnMut(T) -> I,
        I: IntoIterator<Item = T>,
    {
        let mut read_i = 0;
        let mut write_i = 0;
        unsafe {
            let mut old_len = self.len();
            self.set_len(0); // make sure we just leak elements in case of panic

            while read_i < old_len {
                // move the read_i'th item out of the vector and map it
                // to an iterator
                let e = ptr::read(self.as_ptr().add(read_i));
                let iter = f(e).into_iter();
                read_i += 1;

                for e in iter {
                    if write_i < read_i {
                        ptr::write(self.as_mut_ptr().add(write_i), e);
                        write_i += 1;
                    } else {
                        // If this is reached we ran out of space
                        // in the middle of the vector.
                        // However, the vector is in a valid state here,
                        // so we just do a somewhat inefficient insert.
                        self.set_len(old_len);
                        self.insert(write_i, e);

                        old_len = self.len();
                        self.set_len(0);

                        read_i += 1;
                        write_i += 1;
                    }
                }
            }

            // write_i tracks the number of actually written new items.
            self.set_len(write_i);
        }

        self
    }
}
