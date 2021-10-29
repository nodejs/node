# Headers and Man Pages installation target
IF (CARES_INSTALL)
	# ManPages
	FILE (GLOB DevelManPages "." "*.3")
	INSTALL (FILES ${DevelManPages}
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
		COMPONENT Devel
	)

	IF (CARES_BUILD_TOOLS)
		FILE (GLOB ToolManPages "." "*.1")
		INSTALL (FILES ${ToolManPages}
			DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
			COMPONENT Tools
		)
	ENDIF ()
ENDIF ()
