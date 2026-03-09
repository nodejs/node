macro_rules! get_hash {
    ($self:expr) => {
        match $self.tag() {
            DYNAMIC_TAG => {
                let unsafe_data = $self.unsafe_data;
                unsafe { $crate::dynamic::deref_from(unsafe_data) }
                    .header
                    .header
                    .hash
            }
            INLINE_TAG => {
                // This is passed as input to the caller's `Hasher` implementation, so it's okay
                // that this isn't really a hash
                $self.unsafe_data.hash()
            }
            _ => unsafe { debug_unreachable!() },
        }
    };
}

macro_rules! partial_eq {
    ($self:expr, $other:expr) => {
        if $self.unsafe_data == $other.unsafe_data {
            return true;
        }

        // If one is inline and the other is not, the length is different.
        // If one is static and the other is not, it's different.
        if $self.tag() != $other.tag() {
            return false;
        }

        if $self.is_dynamic() && $other.is_dynamic() {
            let te = unsafe { $crate::dynamic::deref_from($self.unsafe_data) };
            let oe = unsafe { $crate::dynamic::deref_from($other.unsafe_data) };

            if te.header.header.hash != oe.header.header.hash {
                return false;
            }

            return te.slice == oe.slice;
        }

        if $self.get_hash() != $other.get_hash() {
            return false;
        }
    };
}

macro_rules! impl_from_alias {
    ($ty:ty) => {
        impl $ty {
            #[inline]
            pub(crate) fn from_alias(alias: TaggedValue) -> Self {
                if alias.tag() & TAG_MASK == DYNAMIC_TAG {
                    unsafe {
                        let arc = $crate::dynamic::restore_arc(alias);
                        forget(arc.clone());
                        forget(arc);
                    }
                }

                Self { unsafe_data: alias }
            }
        }
    };
}

pub(crate) use get_hash;
pub(crate) use impl_from_alias;
pub(crate) use partial_eq;
