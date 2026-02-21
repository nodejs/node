;; expected = R"---(;; This is a polyglot C++/WAT file.
;; Comment lines are ignored and not expected in the disassembler output.
(module
  (memory $memory0 0)
  ;; Function with SIMD type and constant.
  (func $func0 (result v128)
    ;; We always print v128 constants as hexadecimal 4*i32.
    v128.const i32x4 0x00000000 0x00000000 0x00000000 0x00000000
  )
  (func $func1
    ;; SIMD load and stores.
    i32.const 0
    v128.load
    drop
    i32.const 0
    ;; Non-default memargs.
    v128.load8x8_s offset=3
    drop
    i32.const 0
    v128.load8x8_u align=2
    drop
    i32.const 0
    v128.load16x4_s offset=3 align=4
    drop
    i32.const 0
    v128.load16x4_u
    drop
    i32.const 0
    v128.load32x2_s
    drop
    i32.const 0
    v128.load32x2_u
    drop
    i32.const 0
    v128.load8_splat
    drop
    i32.const 0
    v128.load16_splat
    drop
    i32.const 0
    v128.load32_splat
    drop
    i32.const 0
    v128.load64_splat
    drop
    i32.const 0
    v128.load32_zero
    drop
    i32.const 0
    v128.load64_zero
    drop
    i32.const 0
    ;; Call function instead of repeating large immediate(s) all the time.
    call $func0
    v128.store
    i32.const 0
    call $func0
    v128.load8_lane 0
    drop
    i32.const 0
    call $func0
    v128.load16_lane 1
    drop
    i32.const 0
    call $func0
    v128.load32_lane 3
    drop
    i32.const 0
    call $func0
    v128.load64_lane 0
    drop
    i32.const 0
    call $func0
    v128.store16_lane 0
    i32.const 0
    call $func0
    v128.store32_lane 1
    i32.const 0
    call $func0
    v128.store64_lane 0
    ;; Other SIMD instructions.
    call $func0
    call $func0
    i8x16.shuffle 0 1 2 3 0 1 2 3 0 1 2 3 0 1 2 3
    call $func0
    i8x16.swizzle
    drop
    ;; splat
    i32.const 0
    i8x16.splat
    drop
    i32.const 0
    i16x8.splat
    drop
    i32.const 0
    i32x4.splat
    drop
    i64.const 0
    i64x2.splat
    drop
    f32.const 0.0
    f32x4.splat
    drop
    f64.const 0.0
    f64x2.splat
    drop
    ;; extract_lane and replace_lane
    call $func0
    i8x16.extract_lane_s 0
    drop
    call $func0
    i8x16.extract_lane_u 0
    drop
    call $func0
    i16x8.extract_lane_s 0
    drop
    call $func0
    i16x8.extract_lane_u 0
    drop
    call $func0
    i32x4.extract_lane 0
    drop
    call $func0
    i64x2.extract_lane 0
    drop
    call $func0
    f32x4.extract_lane 0
    drop
    call $func0
    f64x2.extract_lane 0
    drop
    call $func0
    i32.const 0
    i8x16.replace_lane 0
    i32.const 0
    i16x8.replace_lane 0
    i32.const 0
    i32x4.replace_lane 0
    i64.const 0
    i64x2.replace_lane 0
    f32.const 0.0
    f32x4.replace_lane 0
    f64.const 0.0
    f64x2.replace_lane 0
    ;; Comparisons: i8
    call $func0
    i8x16.eq
    call $func0
    i8x16.ne
    call $func0
    i8x16.lt_s
    call $func0
    i8x16.lt_u
    call $func0
    i8x16.gt_s
    call $func0
    i8x16.gt_u
    call $func0
    i8x16.le_s
    call $func0
    i8x16.le_u
    call $func0
    i8x16.ge_s
    call $func0
    i8x16.ge_u
    ;; Comparisons: i16
    call $func0
    i16x8.eq
    call $func0
    i16x8.ne
    call $func0
    i16x8.lt_s
    call $func0
    i16x8.lt_u
    call $func0
    i16x8.gt_s
    call $func0
    i16x8.gt_u
    call $func0
    i16x8.le_s
    call $func0
    i16x8.le_u
    call $func0
    i16x8.ge_s
    call $func0
    i16x8.ge_u
    ;; Comparisons: i32
    call $func0
    i32x4.eq
    call $func0
    i32x4.ne
    call $func0
    i32x4.lt_s
    call $func0
    i32x4.lt_u
    call $func0
    i32x4.gt_s
    call $func0
    i32x4.gt_u
    call $func0
    i32x4.le_s
    call $func0
    i32x4.le_u
    call $func0
    i32x4.ge_s
    call $func0
    i32x4.ge_u
    ;; Comparisons: i64
    call $func0
    i64x2.eq
    call $func0
    i64x2.ne
    call $func0
    i64x2.lt_s
    call $func0
    i64x2.gt_s
    call $func0
    i64x2.le_s
    call $func0
    i64x2.ge_s
    ;; Comparisons: f32
    call $func0
    f32x4.eq
    call $func0
    f32x4.ne
    call $func0
    f32x4.lt
    call $func0
    f32x4.gt
    call $func0
    f32x4.le
    call $func0
    f32x4.ge
    ;; Comparisons: f64
    call $func0
    f64x2.eq
    call $func0
    f64x2.ne
    call $func0
    f64x2.lt
    call $func0
    f64x2.gt
    call $func0
    f64x2.le
    call $func0
    f64x2.ge
    ;; Bitwise operations.
    v128.not
    call $func0
    v128.and
    call $func0
    v128.andnot
    call $func0
    v128.or
    call $func0
    v128.xor
    call $func0
    call $func0
    v128.bitselect
    v128.any_true
    drop
    ;; Floating-point demotion and promotions.
    call $func0
    f32x4.demote_f64x2_zero
    f64x2.promote_low_f32x4
    ;; i8 operations.
    i8x16.abs
    i8x16.neg
    i8x16.popcnt
    i8x16.all_true
    drop
    call $func0
    i8x16.bitmask
    drop
    call $func0
    call $func0
    i8x16.narrow_i16x8_s
    call $func0
    i8x16.narrow_i16x8_u
    i32.const 0
    i8x16.shl
    i32.const 0
    i8x16.shr_s
    i32.const 0
    i8x16.shr_u
    call $func0
    i8x16.add
    call $func0
    i8x16.add_sat_s
    call $func0
    i8x16.add_sat_u
    call $func0
    i8x16.sub
    call $func0
    i8x16.sub_sat_s
    call $func0
    i8x16.sub_sat_u
    call $func0
    i8x16.min_s
    call $func0
    i8x16.min_u
    call $func0
    i8x16.max_s
    call $func0
    i8x16.max_u
    call $func0
    i8x16.avgr_u
    ;; i16 operations.
    i16x8.abs
    i16x8.neg
    call $func0
    i16x8.q15mulr_sat_s
    i16x8.all_true
    drop
    call $func0
    i16x8.bitmask
    drop
    call $func0
    call $func0
    i16x8.narrow_i32x4_s
    call $func0
    i16x8.narrow_i32x4_u
    i16x8.extend_low_i8x16_s
    i16x8.extend_high_i8x16_s
    i16x8.extend_low_i8x16_u
    i16x8.extend_high_i8x16_u
    i32.const 0
    i16x8.shl
    i32.const 0
    i16x8.shr_s
    i32.const 0
    i16x8.shr_u
    call $func0
    i16x8.add
    call $func0
    i16x8.add_sat_s
    call $func0
    i16x8.add_sat_u
    call $func0
    i16x8.sub
    call $func0
    i16x8.sub_sat_s
    call $func0
    i16x8.sub_sat_u
    call $func0
    i16x8.mul
    call $func0
    i16x8.min_s
    call $func0
    i16x8.min_u
    call $func0
    i16x8.max_s
    call $func0
    i16x8.max_u
    call $func0
    i16x8.avgr_u
    ;; i32 operations.
    i32x4.abs
    i32x4.neg
    i32x4.all_true
    drop
    call $func0
    i32x4.bitmask
    drop
    call $func0
    i32x4.extend_low_i16x8_s
    i32x4.extend_high_i16x8_s
    i32x4.extend_low_i16x8_u
    i32x4.extend_high_i16x8_u
    i32.const 0
    i32x4.shl
    i32.const 0
    i32x4.shr_s
    i32.const 0
    i32x4.shr_u
    call $func0
    i32x4.add
    call $func0
    i32x4.sub
    call $func0
    i32x4.mul
    call $func0
    i32x4.min_s
    call $func0
    i32x4.min_u
    call $func0
    i32x4.max_s
    call $func0
    i32x4.max_u
    call $func0
    i32x4.dot_i16x8_s
    ;; i64 operations.
    i64x2.abs
    i64x2.neg
    i64x2.all_true
    drop
    call $func0
    i64x2.bitmask
    drop
    call $func0
    i64x2.extend_low_i32x4_s
    i64x2.extend_high_i32x4_s
    i64x2.extend_low_i32x4_u
    i64x2.extend_high_i32x4_u
    i32.const 0
    i64x2.shl
    i32.const 0
    i64x2.shr_s
    i32.const 0
    i64x2.shr_u
    call $func0
    i64x2.add
    call $func0
    i64x2.sub
    call $func0
    i64x2.mul
    ;; f32 operations.
    f32x4.ceil
    f32x4.floor
    f32x4.trunc
    f32x4.nearest
    f32x4.abs
    f32x4.neg
    f32x4.sqrt
    call $func0
    f32x4.add
    call $func0
    f32x4.sub
    call $func0
    f32x4.mul
    call $func0
    f32x4.div
    call $func0
    f32x4.min
    call $func0
    f32x4.max
    call $func0
    f32x4.pmin
    call $func0
    f32x4.pmax
    ;; f64 operations.
    f64x2.ceil
    f64x2.floor
    f64x2.trunc
    f64x2.nearest
    f64x2.abs
    f64x2.neg
    f64x2.sqrt
    call $func0
    f64x2.add
    call $func0
    f64x2.sub
    call $func0
    f64x2.mul
    call $func0
    f64x2.div
    call $func0
    f64x2.min
    call $func0
    f64x2.max
    call $func0
    f64x2.pmin
    call $func0
    f64x2.pmax
    ;; Extended integer arithmetic.
    i16x8.extadd_pairwise_i8x16_s
    i16x8.extadd_pairwise_i8x16_u
    i32x4.extadd_pairwise_i16x8_s
    i32x4.extadd_pairwise_i16x8_u
    call $func0
    i16x8.extmul_low_i8x16_s
    call $func0
    i16x8.extmul_high_i8x16_s
    call $func0
    i16x8.extmul_low_i8x16_u
    call $func0
    i16x8.extmul_high_i8x16_u
    call $func0
    i32x4.extmul_low_i16x8_s
    call $func0
    i32x4.extmul_high_i16x8_s
    call $func0
    i32x4.extmul_low_i16x8_u
    call $func0
    i32x4.extmul_high_i16x8_u
    call $func0
    i64x2.extmul_low_i32x4_s
    call $func0
    i64x2.extmul_high_i32x4_s
    call $func0
    i64x2.extmul_low_i32x4_u
    call $func0
    i64x2.extmul_high_i32x4_u
    ;; Conversions.
    i32x4.trunc_sat_f32x4_s
    i32x4.trunc_sat_f32x4_u
    f32x4.convert_i32x4_s
    f32x4.convert_i32x4_u
    i32x4.trunc_sat_f64x2_s_zero
    i32x4.trunc_sat_f64x2_u_zero
    f64x2.convert_low_i32x4_s
    f64x2.convert_low_i32x4_u
    drop
  )
)
;;)---";
