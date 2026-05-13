set(LLVM_LIBC_SHARED_RPC_EXPORT_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/rpc.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/rpc_util.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/rpc_dispatch.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/rpc_server.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/rpc_opcodes.h"
)

# Install the freestanding RPC interface to the compiler tree.
if(LIBC_TARGET_OS_IS_GPU)
  foreach(header_path IN LISTS LLVM_LIBC_SHARED_RPC_EXPORT_HEADERS)
    install(FILES "${header_path}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/shared"
            COMPONENT libc-headers)
  endforeach()
endif()
