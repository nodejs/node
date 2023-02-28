set(MAN_PAGES
  ZIP_SOURCE_GET_ARGS.3
  libzip.3
  zip_add.3
  zip_add_dir.3
  zip_close.3
  zip_compression_method_supported.3
  zip_delete.3
  zip_dir_add.3
  zip_discard.3
  zip_encryption_method_supported.3
  zip_error_clear.3
  zip_error_code_system.3
  zip_error_code_zip.3
  zip_error_fini.3
  zip_error_get.3
  zip_error_get_sys_type.3
  zip_error_init.3
  zip_error_set.3
  zip_error_strerror.3
  zip_error_system_type.3
  zip_error_to_data.3
  zip_error_to_str.3
  zip_errors.3
  zip_fclose.3
  zip_fdopen.3
  zip_file_add.3
  zip_file_attributes_init.3
  zip_file_extra_field_delete.3
  zip_file_extra_field_get.3
  zip_file_extra_field_set.3
  zip_file_extra_fields_count.3
  zip_file_get_comment.3
  zip_file_get_error.3
  zip_file_get_external_attributes.3
  zip_file_rename.3
  zip_file_set_comment.3
  zip_file_set_encryption.3
  zip_file_set_external_attributes.3
  zip_file_set_mtime.3
  zip_file_strerror.3
  zip_fopen.3
  zip_fopen_encrypted.3
  zip_fread.3
  zip_fseek.3
  zip_ftell.3
  zip_get_archive_comment.3
  zip_get_archive_flag.3
  zip_get_error.3
  zip_get_file_comment.3
  zip_get_name.3
  zip_get_num_entries.3
  zip_get_num_files.3
  zip_libzip_version.3
  zip_name_locate.3
  zip_open.3
  zip_register_cancel_callback_with_state.3
  zip_register_progress_callback.3
  zip_register_progress_callback_with_state.3
  zip_rename.3
  zip_set_archive_comment.3
  zip_set_archive_flag.3
  zip_set_default_password.3
  zip_set_file_comment.3
  zip_set_file_compression.3
  zip_source.3
  zip_source_begin_write.3
  zip_source_buffer.3
  zip_source_buffer_fragment.3
  zip_source_close.3
  zip_source_commit_write.3
  zip_source_error.3
  zip_source_file.3
  zip_source_filep.3
  zip_source_free.3
  zip_source_function.3
  zip_source_is_deleted.3
  zip_source_layered.3
  zip_source_keep.3
  zip_source_make_command_bitmap.3
  zip_source_open.3
  zip_source_read.3
  zip_source_rollback_write.3
  zip_source_seek.3
  zip_source_seek_compute_offset.3
  zip_source_seek_write.3
  zip_source_stat.3
  zip_source_tell.3
  zip_source_tell_write.3
  zip_source_win32a.3
  zip_source_win32handle.3
  zip_source_win32w.3
  zip_source_window.3
  zip_source_write.3
  zip_source_zip.3
  zip_stat.3
  zip_stat_init.3
  zip_unchange.3
  zip_unchange_all.3
  zip_unchange_archive.3
  zipcmp.1
  zipmerge.1
  ziptool.1
)

foreach(MAN_PAGE ${MAN_PAGES})
  string(REGEX REPLACE "[1-9]$" "${DOCUMENTATION_FORMAT}" SOURCE_FILE ${MAN_PAGE})
  if(LIBZIP_DO_INSTALL)
    if (DOCUMENTATION_FORMAT MATCHES "html")
      install(FILES ${PROJECT_BINARY_DIR}/man/${MAN_PAGE} DESTINATION ${CMAKE_INSTALL_DOCDIR}/${PROJECT_NAME} RENAME ${SOURCE_FILE})
    else()
      string(REGEX REPLACE ".*(.)$" "man\\1" SUBDIR ${MAN_PAGE})
      install(FILES ${PROJECT_BINARY_DIR}/man/${MAN_PAGE} DESTINATION ${CMAKE_INSTALL_MANDIR}/${SUBDIR})
    endif()
  endif()
  # configure_file does not find out about updates to the sources, and it does not provide a target
  #configure_file(${SOURCE_FILE} ${MAN_PAGE} COPYONLY)
  add_custom_command(OUTPUT ${MAN_PAGE}
    DEPENDS ${SOURCE_FILE}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_FILE} ${CMAKE_CURRENT_BINARY_DIR}/${MAN_PAGE}
    COMMENT "Preparing ${MAN_PAGE}"
    )

  string(REGEX REPLACE "[1-9]$" "html" HTML_FILE ${MAN_PAGE})
  string(REGEX REPLACE "[1-9]$" "man" MAN_FILE ${MAN_PAGE})
  string(REGEX REPLACE "[1-9]$" "mdoc" MDOC_FILE ${MAN_PAGE})

  # html re-generation
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${HTML_FILE}
    DEPENDS ${MDOC_FILE}
    COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_CURRENT_SOURCE_DIR}/${MDOC_FILE} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${HTML_FILE} -P ${CMAKE_CURRENT_SOURCE_DIR}/update-html.cmake
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/${HTML_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${HTML_FILE}
    )
  list(APPEND UPDATEHTML ${CMAKE_CURRENT_BINARY_DIR}/${HTML_FILE})

  # man re-generation
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MAN_FILE}
    DEPENDS ${MDOC_FILE}
     COMMAND ${CMAKE_COMMAND} -DIN=${CMAKE_CURRENT_SOURCE_DIR}/${MDOC_FILE} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${MAN_FILE} -P ${CMAKE_CURRENT_SOURCE_DIR}/update-man.cmake
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/${MAN_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${MAN_FILE}
    )
  list(APPEND UPDATEMAN ${CMAKE_CURRENT_BINARY_DIR}/${MAN_FILE})
endforeach()
add_custom_target(man ALL DEPENDS ${MAN_PAGES})
add_custom_target(update-man DEPENDS ${UPDATEMAN})
add_custom_target(update-html DEPENDS ${UPDATEHTML})

file(STRINGS links MANPAGE_LINKS)
foreach(LINKS_LINE ${MANPAGE_LINKS})
  if(${LINKS_LINE} MATCHES "(.*) (.*)")
    set(SOURCE ${CMAKE_MATCH_1})
    set(TARGET ${CMAKE_MATCH_2})
    if(LIBZIP_DO_INSTALL)
      if (DOCUMENTATION_FORMAT MATCHES "html")
	INSTALL(FILES ${PROJECT_BINARY_DIR}/man/${SOURCE}.3 DESTINATION ${CMAKE_INSTALL_DOCDIR}/${PROJECT_NAME} RENAME ${TARGET}.html)
      else()
	INSTALL(FILES ${PROJECT_BINARY_DIR}/man/${SOURCE}.3 DESTINATION ${CMAKE_INSTALL_MANDIR}/man3 RENAME ${TARGET}.3)
      endif()
    endif()
  endif()
endforeach()

add_custom_target(update_zip_errors
  COMMAND sh ${PROJECT_SOURCE_DIR}/man/make_zip_errors.sh ${CMAKE_SOURCE_DIR}/lib/zip.h ${PROJECT_SOURCE_DIR}/man/zip_errors.mdoc
  DEPENDS ${CMAKE_SOURCE_DIR}/lib/zip.h ${PROJECT_SOURCE_DIR}/man/zip_errors.mdoc
)
