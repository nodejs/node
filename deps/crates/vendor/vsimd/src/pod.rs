use crate::vector::{V128, V256, V512, V64};

pub unsafe trait POD: Copy + 'static {
    const ID: PodTypeId;
}

macro_rules! mark_pod {
    ($($ty:ident),*) => {
        $(
            unsafe impl POD for $ty {
                const ID: PodTypeId = PodTypeId::$ty;
            }
        )*
    };
}

mark_pod!(u8, u16, u32, u64, u128, usize);
mark_pod!(i8, i16, i32, i64, i128, isize);
mark_pod!(f32, f64);
mark_pod!(V64, V128, V256, V512);

#[inline(always)]
pub fn align<T: POD, U: POD>(slice: &[T]) -> (&[T], &[U], &[T]) {
    unsafe { slice.align_to() }
}

#[allow(non_camel_case_types)]
#[derive(Debug, Clone, Copy)]
pub enum PodTypeId {
    u8,
    u16,
    u32,
    u64,
    u128,
    usize,

    i8,
    i16,
    i32,
    i64,
    i128,
    isize,

    f32,
    f64,

    V64,
    V128,
    V256,
    V512,
}

#[macro_export]
macro_rules! is_pod_type {
    ($self:ident, $x:ident $(| $xs:ident)*) => {{
        // TODO: inline const
        use $crate::pod::POD;
        struct IsPodType<T>(T);
        impl <T: POD> IsPodType<T> {
            const VALUE: bool = { matches!(<T as POD>::ID, $crate::pod::PodTypeId::$x $(| $crate::pod::PodTypeId::$xs)*) };
        }
        IsPodType::<$self>::VALUE
    }};
}
