use swc_common::Mark;
use swc_ecma_transforms_base::resolver;
use swc_ecma_transforms_compat::es2015::arrow;
use swc_ecma_transforms_testing::test;

use super::*;

fn tr() -> impl Pass {
    jsx_self(true)
}

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |_| tr(),
    basic_sample,
    r#"var x = <sometag />"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |_| tr(),
    disable_with_super,
    r#"class A { }

class B extends A {
  constructor() {
    <sometag1 />;
    super(<sometag2 />);
    <sometag3 />;
  }
}

class C {
  constructor() {
    <sometag4 />;
    class D extends A {
      constructor() {
        super();
      }
    }
    const E = class extends A {
      constructor() {
        super();
      }
    };
  }
}

class E extends A {
  constructor() {
    this.x = () => <sometag5 />;
    this.y = function () {
      return <sometag6 />;
    };
    function z() {
      return <sometag7 />;
    }
    { <sometag8 /> }
    super();
  }
}

class F {
  constructor() {
    <sometag9 />
  }
}

class G extends A {
  constructor() {
    return <sometag10 />;
  }
}"#
);

test!(
    module,
    ::swc_ecma_parser::Syntax::Es(::swc_ecma_parser::EsSyntax {
        jsx: true,
        ..Default::default()
    }),
    |_| {
        let unresolved_mark = Mark::new();
        let top_level_mark = Mark::new();
        (
            resolver(unresolved_mark, top_level_mark, false),
            tr(),
            arrow(unresolved_mark),
        )
    },
    arrow_function,
    r#"<div />;
() => <div />;

function fn() {
  <div />;
  () => <div />;
}"#
);
