#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "wasm.h"

#define own

// Print a Wasm value
void wasm_val_print(wasm_val_t val) {
  switch (val.kind) {
    case WASM_I32: {
      printf("%" PRIu32, val.of.i32);
    } break;
    case WASM_I64: {
      printf("%" PRIu64, val.of.i64);
    } break;
    case WASM_F32: {
      printf("%f", val.of.f32);
    } break;
    case WASM_F64: {
      printf("%g", val.of.f64);
    } break;
    case WASM_ANYREF:
    case WASM_FUNCREF: {
      if (val.of.ref == NULL) {
        printf("null");
      } else {
        printf("ref(%p)", val.of.ref);
      }
    } break;
  }
}

// A function to be called from Wasm code.
own wasm_trap_t* print_callback(
  const wasm_val_t args[], wasm_val_t results[]
) {
  printf("Calling back...\n> ");
  wasm_val_print(args[0]);
  printf("\n");

  wasm_val_copy(&results[0], &args[0]);
  return NULL;
}


// A function closure.
own wasm_trap_t* closure_callback(
  void* env, const wasm_val_t args[], wasm_val_t results[]
) {
  int i = *(int*)env;
  printf("Calling back closure...\n");
  printf("> %d\n", i);

  results[0].kind = WASM_I32;
  results[0].of.i32 = (int32_t)i;
  return NULL;
}


int main(int argc, const char* argv[]) {
  // Initialize.
  printf("Initializing...\n");
  wasm_engine_t* engine = wasm_engine_new();
  wasm_store_t* store = wasm_store_new(engine);

  // Load binary.
  printf("Loading binary...\n");
  FILE* file = fopen("callback.wasm", "r");
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

  // Create external print functions.
  printf("Creating callback...\n");
  own wasm_functype_t* print_type = wasm_functype_new_1_1(wasm_valtype_new_i32(), wasm_valtype_new_i32());
  own wasm_func_t* print_func = wasm_func_new(store, print_type, print_callback);

  int i = 42;
  own wasm_functype_t* closure_type = wasm_functype_new_0_1(wasm_valtype_new_i32());
  own wasm_func_t* closure_func = wasm_func_new_with_env(store, closure_type, closure_callback, &i, NULL);

  wasm_functype_delete(print_type);
  wasm_functype_delete(closure_type);

  // Instantiate.
  printf("Instantiating module...\n");
  const wasm_extern_t* imports[] = {
    wasm_func_as_extern(print_func), wasm_func_as_extern(closure_func)
  };
  own wasm_instance_t* instance = wasm_instance_new(store, module, imports);
  if (!instance) {
    printf("> Error instantiating module!\n");
    return 1;
  }

  wasm_func_delete(print_func);
  wasm_func_delete(closure_func);

  // Extract export.
  printf("Extracting export...\n");
  own wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  if (exports.size == 0) {
    printf("> Error accessing exports!\n");
    return 1;
  }
  const wasm_func_t* run_func = wasm_extern_as_func(exports.data[0]);
  if (run_func == NULL) {
    printf("> Error accessing export!\n");
    return 1;
  }

  wasm_module_delete(module);
  wasm_instance_delete(instance);

  // Call.
  printf("Calling export...\n");
  wasm_val_t args[2];
  args[0].kind = WASM_I32;
  args[0].of.i32 = 3;
  args[1].kind = WASM_I32;
  args[1].of.i32 = 4;
  wasm_val_t results[1];
  if (wasm_func_call(run_func, args, results)) {
    printf("> Error calling function!\n");
    return 1;
  }

  wasm_extern_vec_delete(&exports);

  // Print result.
  printf("Printing result...\n");
  printf("> %u\n", results[0].of.i32);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_engine_delete(engine);

  // All done.
  printf("Done.\n");
  return 0;
}
