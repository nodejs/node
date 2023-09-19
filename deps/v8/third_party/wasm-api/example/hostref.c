#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "wasm.h"

#define own


// A function to be called from Wasm code.
own wasm_trap_t* callback(
  const wasm_val_t args[], wasm_val_t results[]
) {
  printf("Calling back...\n> ");
  printf("> %p\n",
    args[0].of.ref ? wasm_ref_get_host_info(args[0].of.ref) : NULL);
  wasm_val_copy(&results[0], &args[0]);
  return NULL;
}


wasm_func_t* get_export_func(const wasm_extern_vec_t* exports, size_t i) {
  if (exports->size <= i || !wasm_extern_as_func(exports->data[i])) {
    printf("> Error accessing function export %zu!\n", i);
    exit(1);
  }
  return wasm_extern_as_func(exports->data[i]);
}

wasm_global_t* get_export_global(const wasm_extern_vec_t* exports, size_t i) {
  if (exports->size <= i || !wasm_extern_as_global(exports->data[i])) {
    printf("> Error accessing global export %zu!\n", i);
    exit(1);
  }
  return wasm_extern_as_global(exports->data[i]);
}

wasm_table_t* get_export_table(const wasm_extern_vec_t* exports, size_t i) {
  if (exports->size <= i || !wasm_extern_as_table(exports->data[i])) {
    printf("> Error accessing table export %zu!\n", i);
    exit(1);
  }
  return wasm_extern_as_table(exports->data[i]);
}


own wasm_ref_t* call_v_r(const wasm_func_t* func) {
  printf("call_v_r... "); fflush(stdout);
  wasm_val_t results[1];
  if (wasm_func_call(func, NULL, results)) {
    printf("> Error calling function!\n");
    exit(1);
  }
  printf("okay\n");
  return results[0].of.ref;
}

void call_r_v(const wasm_func_t* func, wasm_ref_t* ref) {
  printf("call_r_v... "); fflush(stdout);
  wasm_val_t args[1];
  args[0].kind = WASM_ANYREF;
  args[0].of.ref = ref;
  if (wasm_func_call(func, args, NULL)) {
    printf("> Error calling function!\n");
    exit(1);
  }
  printf("okay\n");
}

own wasm_ref_t* call_r_r(const wasm_func_t* func, wasm_ref_t* ref) {
  printf("call_r_r... "); fflush(stdout);
  wasm_val_t args[1];
  args[0].kind = WASM_ANYREF;
  args[0].of.ref = ref;
  wasm_val_t results[1];
  if (wasm_func_call(func, args, results)) {
    printf("> Error calling function!\n");
    exit(1);
  }
  printf("okay\n");
  return results[0].of.ref;
}

void call_ir_v(const wasm_func_t* func, int32_t i, wasm_ref_t* ref) {
  printf("call_ir_v... "); fflush(stdout);
  wasm_val_t args[2];
  args[0].kind = WASM_I32;
  args[0].of.i32 = i;
  args[1].kind = WASM_ANYREF;
  args[1].of.ref = ref;
  if (wasm_func_call(func, args, NULL)) {
    printf("> Error calling function!\n");
    exit(1);
  }
  printf("okay\n");
}

own wasm_ref_t* call_i_r(const wasm_func_t* func, int32_t i) {
  printf("call_i_r... "); fflush(stdout);
  wasm_val_t args[1];
  args[0].kind = WASM_I32;
  args[0].of.i32 = i;
  wasm_val_t results[1];
  if (wasm_func_call(func, args, results)) {
    printf("> Error calling function!\n");
    exit(1);
  }
  printf("okay\n");
  return results[0].of.ref;
}

void check(own wasm_ref_t* actual, const wasm_ref_t* expected) {
  if (actual != expected &&
      !(actual && expected && wasm_ref_same(actual, expected))) {
    printf("> Error reading reference, expected %p, got %p\n",
      expected ? wasm_ref_get_host_info(expected) : NULL,
      actual ? wasm_ref_get_host_info(actual) : NULL);
    exit(1);
  }
  if (actual) wasm_ref_delete(actual);
}


int main(int argc, const char* argv[]) {
  // Initialize.
  printf("Initializing...\n");
  wasm_engine_t* engine = wasm_engine_new();
  wasm_store_t* store = wasm_store_new(engine);

  // Load binary.
  printf("Loading binary...\n");
  FILE* file = fopen("hostref.wasm", "r");
  if (!file) {
    printf("> Error loading module!\n");
    return 1;
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_t binary;
  wasm_byte_vec_new_uninitialized(&binary, file_size);
  if (fread(binary.data, file_size, 1, file) != 1) {
    printf("> Error loading module!\n");
    return 1;
  }
  fclose(file);

  // Compile.
  printf("Compiling module...\n");
  own wasm_module_t* module = wasm_module_new(store, &binary);
  if (!module) {
    printf("> Error compiling module!\n");
    return 1;
  }

  wasm_byte_vec_delete(&binary);

  // Create external callback function.
  printf("Creating callback...\n");
  own wasm_functype_t* callback_type = wasm_functype_new_1_1(
    wasm_valtype_new(WASM_ANYREF), wasm_valtype_new(WASM_ANYREF));
  own wasm_func_t* callback_func =
    wasm_func_new(store, callback_type, callback);

  wasm_functype_delete(callback_type);

  // Instantiate.
  printf("Instantiating module...\n");
  const wasm_extern_t* imports[] = { wasm_func_as_extern(callback_func) };
  own wasm_instance_t* instance =
    wasm_instance_new(store, module, imports, NULL);
  if (!instance) {
    printf("> Error instantiating module!\n");
    return 1;
  }

  wasm_func_delete(callback_func);
  wasm_module_delete(module);

  // Extract export.
  printf("Extracting exports...\n");
  own wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  size_t i = 0;
  wasm_global_t* global = get_export_global(&exports, i++);
  wasm_table_t* table = get_export_table(&exports, i++);
  wasm_func_t* global_set = get_export_func(&exports, i++);
  wasm_func_t* global_get = get_export_func(&exports, i++);
  wasm_func_t* table_set = get_export_func(&exports, i++);
  wasm_func_t* table_get = get_export_func(&exports, i++);
  wasm_func_t* func_call = get_export_func(&exports, i++);

  wasm_instance_delete(instance);

  // Create host references.
  printf("Creating host references...\n");
  own wasm_ref_t* host1 = wasm_foreign_as_ref(wasm_foreign_new(store));
  own wasm_ref_t* host2 = wasm_foreign_as_ref(wasm_foreign_new(store));
  wasm_ref_set_host_info(host1, (void*)1);
  wasm_ref_set_host_info(host2, (void*)2);

  // Some sanity checks.
  check(NULL, NULL);
  check(wasm_ref_copy(host1), host1);
  check(wasm_ref_copy(host2), host2);

  own wasm_val_t val;
  val.kind = WASM_ANYREF;
  val.of.ref = wasm_ref_copy(host1);
  check(wasm_ref_copy(val.of.ref), host1);
  own wasm_ref_t* ref = val.of.ref;
  check(wasm_ref_copy(ref), host1);
  wasm_val_delete(&val);

  // Interact.
  printf("Accessing global...\n");
  check(call_v_r(global_get), NULL);
  call_r_v(global_set, host1);
  check(call_v_r(global_get), host1);
  call_r_v(global_set, host2);
  check(call_v_r(global_get), host2);
  call_r_v(global_set, NULL);
  check(call_v_r(global_get), NULL);

  wasm_global_get(global, &val);
  assert(val.kind == WASM_ANYREF);
  check(val.of.ref, NULL);
  val.of.ref = host2;
  wasm_global_set(global, &val);
  check(call_v_r(global_get), host2);
  wasm_global_get(global, &val);
  assert(val.kind == WASM_ANYREF);
  check(val.of.ref, host2);

  printf("Accessing table...\n");
  check(call_i_r(table_get, 0), NULL);
  check(call_i_r(table_get, 1), NULL);
  call_ir_v(table_set, 0, host1);
  call_ir_v(table_set, 1, host2);
  check(call_i_r(table_get, 0), host1);
  check(call_i_r(table_get, 1), host2);
  call_ir_v(table_set, 0, NULL);
  check(call_i_r(table_get, 0), NULL);

  check(wasm_table_get(table, 2), NULL);
  wasm_table_set(table, 2, host1);
  check(call_i_r(table_get, 2), host1);
  check(wasm_table_get(table, 2), host1);

  printf("Accessing function...\n");
  check(call_r_r(func_call, NULL), NULL);
  check(call_r_r(func_call, host1), host1);
  check(call_r_r(func_call, host2), host2);

  wasm_ref_delete(host1);
  wasm_ref_delete(host2);

  wasm_extern_vec_delete(&exports);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_engine_delete(engine);

  // All done.
  printf("Done.\n");
  return 0;
}
