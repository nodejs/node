#
# docs
#

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/doc)

set(node_binary ${PROJECT_BINARY_DIR}/default/node)
set(doctool tools/doctool/doctool.js)
set(changelog_html ${PROJECT_BINARY_DIR}/doc/changelog.html)

file(GLOB_RECURSE doc_sources RELATIVE ${PROJECT_SOURCE_DIR} doc/*)

foreach(FILE ${doc_sources})
  string(REGEX REPLACE "(.*)api_assets(.*)" "\\1api/assets\\2" OUT_FILE ${FILE})
  add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/${OUT_FILE}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/${FILE} ${PROJECT_BINARY_DIR}/${OUT_FILE}
    DEPENDS ${PROJECT_SOURCE_DIR}/${FILE}
    )
  list(APPEND doc_sources_copy ${PROJECT_BINARY_DIR}/${OUT_FILE})
endforeach()

file(GLOB_RECURSE api_markdown RELATIVE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/doc/api/*)

foreach(file ${api_markdown})
  string(REGEX REPLACE "(.*)\\.markdown" "\\1" tmp ${file})
  set(api_basenames ${api_basenames} ${tmp})
endforeach()

foreach(api ${api_basenames})
  set(api_html ${api_html} ${PROJECT_BINARY_DIR}/${api}.html)
  add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/${api}.html"
    COMMAND ${node_binary} ${doctool} ${PROJECT_BINARY_DIR}/doc/template.html "${PROJECT_BINARY_DIR}/${api}.markdown" > "${PROJECT_BINARY_DIR}/${api}.html"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    DEPENDS node ${doctool} ${doc_sources_copy}
    VERBATIM
    )
endforeach()

add_custom_target(
  doc
  DEPENDS node ${doc_sources_copy} ${api_html} ${changelog_html}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  )

#add_custom_command(
#  OUTPUT ${PROJECT_BINARY_DIR}/doc/api.html
#  COMMAND ${PROJECT_BINARY_DIR}/default/node tools/ronnjs/bin/ronn.js --fragment doc/api.markdown
#    | sed "s/<h2>\\\(.*\\\)<\\/h2>/<h2 id=\"\\1\">\\1<\\/h2>/g"
#    | cat doc/api_header.html - doc/api_footer.html > ${PROJECT_BINARY_DIR}/doc/api.html
#  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
#  DEPENDS node doc/api.markdown doc/api_header.html doc/api_footer.html
#  VERBATIM
#  )

add_custom_command(
  OUTPUT ${changelog_html}
  COMMAND ${node_binary} ${doctool} doc/template.html ChangeLog
    | sed "s|assets/|api/assets/|g"
    | sed "s|<body>|<body id=\"changelog\">|g" > ${changelog_html}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  DEPENDS ChangeLog node ${doctool} ${doc_sources_copy}
  VERBATIM
  )

#add_custom_command(
#  OUTPUT ${PROJECT_BINARY_DIR}/doc/changelog.html
#  COMMAND cat doc/changelog_header.html ChangeLog doc/changelog_footer.html > ${PROJECT_BINARY_DIR}/doc/changelog.html
#  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
#  DEPENDS ChangeLog doc/changelog_header.html doc/changelog_footer.html
#  VERBATIM
#  )

#add_custom_command(
#  OUTPUT ${PROJECT_BINARY_DIR}/doc/node.1
#  COMMAND ${PROJECT_BINARY_DIR}/default/node tools/ronnjs/bin/ronn.js --roff doc/api.markdown > ${PROJECT_BINARY_DIR}/doc/node.1
#  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
#  DEPENDS node doc/api.markdown tools/ronnjs/bin/ronn.js
#  VERBATIM
#  )
