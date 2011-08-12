#
# package
#

# Allow absolute paths when installing
# see http://www.cmake.org/pipermail/cmake/2008-July/022958.html
set(CPACK_SET_DESTDIR "ON")

if(${node_platform} MATCHES darwin)
  set(CPACK_GENERATOR "TGZ;PackageMaker")
  # CPack requires the files to end in .txt
  configure_file(LICENSE ${PROJECT_BINARY_DIR}/LICENSE.txt COPYONLY)
  configure_file(ChangeLog ${PROJECT_BINARY_DIR}/ChangeLog.txt COPYONLY)
  set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_BINARY_DIR}/LICENSE.txt")
  set(CPACK_RESOURCE_FILE_README "${PROJECT_BINARY_DIR}/ChangeLog.txt")
  #set(CPACK_RESOURCE_FILE_WELCOME "")
elseif(${node_platform} MATCHES linux)
  set(CPACK_GENERATOR "TGZ;DEB;RPM")
else()
  set(CPACK_GENERATOR "TGZ")
endif()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tom Hughes <tom.hughes@palm.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Evented I/O for V8 JavaScript.")
set(CPACK_PACKAGE_DESCRIPTION "Evented I/O for V8 JavaScript.
 Node's goal is to provide an easy way to build scalable network programs.
 Node is similar in design to and influenced by systems like Ruby's Event
 Machine or Python's Twisted. Node takes the event model a bit further—it
 presents the event loop as a language construct instead of as a library.")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_DEBIAN_PACKAGE_SECTION "web")
file(READ ${PROJECT_SOURCE_DIR}/src/node_version.h node_version_h OFFSET 0)
string(REGEX REPLACE ".*NODE_MAJOR_VERSION[ ]*([0-9]+).*" "\\1" CPACK_PACKAGE_VERSION_MAJOR "${node_version_h}")
string(REGEX REPLACE ".*NODE_MINOR_VERSION[ ]*([0-9]+).*" "\\1" CPACK_PACKAGE_VERSION_MINOR "${node_version_h}")
string(REGEX REPLACE ".*NODE_PATCH_VERSION[ ]*([0-9]+).*" "\\1" CPACK_PACKAGE_VERSION_PATCH "${node_version_h}")
set(node_version_string "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

# Note: this is intentionally at the bottom so that the above CPACK variables
# are used by CPack.
include(CPack)
