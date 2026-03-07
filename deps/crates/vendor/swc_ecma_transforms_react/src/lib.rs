#![deny(clippy::all)]
#![allow(clippy::mutable_key_type)]
#![allow(clippy::arc_with_non_send_sync)]
#![allow(rustc::untranslatable_diagnostic_trivial)]
#![cfg_attr(not(feature = "concurrent"), allow(unused))]

use swc_common::{comments::Comments, sync::Lrc, Mark, SourceMap};
use swc_ecma_ast::Pass;
use swc_ecma_hooks::{CompositeHook, VisitMutWithHook};
use swc_ecma_visit::visit_mut_pass;

pub use self::{jsx::*, refresh::options::RefreshOptions};

mod display_name;
pub mod jsx;
mod jsx_self;
mod jsx_src;
mod pure_annotations;
mod refresh;

// Re-export old function names for compatibility
pub fn display_name() -> impl Pass {
    visit_mut_pass(VisitMutWithHook {
        hook: display_name::hook(),
        context: (),
    })
}

pub fn jsx_self(dev: bool) -> impl Pass {
    visit_mut_pass(VisitMutWithHook {
        hook: jsx_self::hook(dev),
        context: (),
    })
}

pub fn jsx_src(dev: bool, cm: Lrc<SourceMap>) -> impl Pass {
    visit_mut_pass(VisitMutWithHook {
        hook: jsx_src::hook(dev, cm),
        context: (),
    })
}

pub fn pure_annotations<C>(comments: Option<C>) -> impl Pass
where
    C: Comments,
{
    visit_mut_pass(VisitMutWithHook {
        hook: pure_annotations::hook(comments),
        context: (),
    })
}

pub fn refresh<C: Comments>(
    dev: bool,
    options: Option<RefreshOptions>,
    cm: Lrc<SourceMap>,
    comments: Option<C>,
    global_mark: Mark,
) -> impl Pass {
    visit_mut_pass(VisitMutWithHook {
        hook: refresh::hook(dev, options, cm, comments, global_mark),
        context: (),
    })
}

/// `@babel/preset-react`
///
/// Preset for all React plugins.
///
///
/// `top_level_mark` should be [Mark] passed to
/// [swc_ecma_transforms_base::resolver::resolver_with_mark].
///
///
///
/// # Note
///
/// This pass uses [swc_ecma_utils::HANDLER].
pub fn react<C>(
    cm: Lrc<SourceMap>,
    comments: Option<C>,
    mut options: Options,
    top_level_mark: Mark,
    unresolved_mark: Mark,
) -> impl Pass
where
    C: Comments + Clone,
{
    let Options { development, .. } = options;
    let development = development.unwrap_or(false);

    let refresh_options = options.refresh.take();

    let hook = CompositeHook {
        first: jsx_src::hook(development, cm.clone()),
        second: CompositeHook {
            first: jsx_self::hook(development),
            second: CompositeHook {
                first: refresh::hook(
                    development,
                    refresh_options.clone(),
                    cm.clone(),
                    comments.clone(),
                    top_level_mark,
                ),
                second: CompositeHook {
                    first: jsx::hook(
                        cm.clone(),
                        comments.clone(),
                        options,
                        top_level_mark,
                        unresolved_mark,
                    ),
                    second: CompositeHook {
                        first: display_name::hook(),
                        second: pure_annotations::hook(comments.clone()),
                    },
                },
            },
        },
    };

    visit_mut_pass(VisitMutWithHook { hook, context: () })
}
