#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "wasm.h"

#define own

const int iterations = 100000;

void finalize(void* data) {
  int i = (int)data;
  if (i % (iterations / 10) == 0) printf("Finalizing #%d...\n", i);
}

int main(int argc, const char* argv[]) {
  // Initialize.
  printf("Initializing...\n");
  wasm_engine_t* engine = wasm_engine_new();
  wasm_store_t* store = wasm_store_new(engine);

  // Load binary.
  printf("Loading binary...\n");
  FILE* file = fopen("finalize.wasm", "r");
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

  // Instantiate.
  printf("Instantiating modules...\n");
  for (int i = 0; i <= iterations; ++i) {
    if (i % (iterations / 10) == 0) printf("%d\n", i);
    own wasm_instance_t* instance = wasm_instance_new(store, module, NULL);
    if (!instance) {
      printf("> Error instantiating module %d!\n", i);
      return 1;
    }
    void* data = (void*)(intptr_t)i;
    wasm_instance_set_host_info_with_finalizer(instance, data, &finalize);
    wasm_instance_delete(instance);
  }

  wasm_module_delete(module);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_engine_delete(engine);

  // All done.
  printf("Done.\n");
  return 0;
}
