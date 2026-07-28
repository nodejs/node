// Note: generic functions are marked `#[inline]` because, even though generic functions are
// typically inlined, this does not seem to always be the case.

mod ceil;
mod copysign;
mod fabs;
mod fdim;
mod floor;
mod fma;
mod fma_wide;
mod fmax;
mod fmaximum;
mod fmaximum_num;
mod fmin;
mod fminimum;
mod fminimum_num;
mod fmod;
mod rint;
mod round;
mod scalbn;
mod sqrt;
mod trunc;

pub use ceil::ceil;
pub use copysign::copysign;
pub use fabs::fabs;
pub use fdim::fdim;
pub use floor::floor;
pub use fma::fma_round;
pub use fma_wide::fma_wide_round;
pub use fmax::fmax;
pub use fmaximum::fmaximum;
pub use fmaximum_num::fmaximum_num;
pub use fmin::fmin;
pub use fminimum::fminimum;
pub use fminimum_num::fminimum_num;
pub use fmod::fmod;
pub use rint::rint_round;
pub use round::round;
pub use scalbn::scalbn;
pub use sqrt::sqrt;
pub use trunc::trunc;
