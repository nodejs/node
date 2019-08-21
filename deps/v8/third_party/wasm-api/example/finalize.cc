#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <cinttypes>

#include "wasm.hh"


const int iterations = 100000;

void finalize(void* data) {
  intptr_t i = reinterpret_cast<intptr_t>(data);
  if (i % (iterations / 10) == 0) {
    std::cout << "Finalizing #" << i << "..." << std::endl;
  }
}

void run() {
  // Initialize.
  std::cout << "Initializing..." << std::endl;
  auto engine = wasm::Engine::make();
  auto store_ = wasm::Store::make(engine.get());
  auto store = store_.get();

  // Load binary.
  std::cout << "Loading binary..." << std::endl;
  std::ifstream file("finalize.wasm");
  file.seekg(0, std::ios_base::end);
  auto file_size = file.tellg();
  file.seekg(0);
  auto binary = wasm::vec<byte_t>::make_uninitialized(file_size);
  file.read(binary.get(), file_size);
  file.close();
  if (file.fail()) {
    std::cout << "> Error loading module!" << std::endl;
    return;
  }

  // Compile.
  std::cout << "Compiling module..." << std::endl;
  auto module = wasm::Module::make(store, binary);
  if (!module) {
    std::cout << "> Error compiling module!" << std::endl;
    return;
  }

  // Instantiate.
  std::cout << "Instantiating modules..." << std::endl;
  for (int i = 0; i <= iterations; ++i) {
    if (i % (iterations / 10) == 0) std::cout << i << std::endl;
    auto instance = wasm::Instance::make(store, module.get(), nullptr);
    if (!instance) {
      std::cout << "> Error instantiating module " << i << "!" << std::endl;
      return;
    }
    instance->set_host_info(reinterpret_cast<void*>(i), &finalize);
  }

  // Shut down.
  std::cout << "Shutting down..." << std::endl;
}


int main(int argc, const char* argv[]) {
  run();
  std::cout << "Done." << std::endl;
  return 0;
}

