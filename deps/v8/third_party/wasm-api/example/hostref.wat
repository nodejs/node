(module
  (import "" "f" (func $fun (param anyref) (result anyref)))

  (global $glob (export "global") (mut anyref) (ref.null))
  (table $tab (export "table") 10 anyref)

  (func (export "global.set") (param $r anyref)
    (global.set $glob (local.get $r))
  )
  (func (export "global.get") (result anyref)
    (global.get $glob)
  )

  (func (export "table.set") (param $i i32) (param $r anyref)
    (table.set $tab (local.get $i) (local.get $r))
  )
  (func (export "table.get") (param $i i32) (result anyref)
    (table.get $tab (local.get $i))
  )

  (func (export "func.call") (param $r anyref) (result anyref)
    (call $fun (local.get $r))
  )
)
