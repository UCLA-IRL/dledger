(module
  (import "" "memory"   (memory 1))
  (import "" "getBlock" (func $getBlock (param i32) (result i32)))
  (import "" "setBlock" (func $setBlock (param i32 i32) (result i32)))

  (func (export "runFunc") (param i32) (result i32)
    get_local 0
    call $getBlock
  )
)