PROJECT(ashunt C)

SET(BUILD_STRING "generic")

FIND_PACKAGE(LibGeoIP)
FIND_PACKAGE(Threads)

IF (LIBGEOIP_FOUND AND CMAKE_HAVE_PTHREAD_CREATE)
	ADD_EXECUTABLE(${PROJECT_NAME}	../xmalloc.c
					../strlcpy.c
					../write_or_die.c
					../misc.c
					../netdev.c
					../aslookup.c
					../bpf.c
					../mtrand.c
					../tprintf.c
					../parser.c
					../ring_rx.c
					../ashunt.c)

	ADD_DEFINITIONS(-DPROGNAME_STRING="${PROJECT_NAME}"
			-DVERSION_STRING="${VERSION}"
			-DBUILD_STRING="${BUILD_STRING}")

	TARGET_LINK_LIBRARIES(ashunt ${LibGeoIP_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
	INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${EXECUTABLE_INSTALL_PATH})
	INSTALL(FILES ../conf/whois.conf DESTINATION ${CONFIG_INSTALL_PATH})
ELSE (LIBGEOIP_FOUND AND CMAKE_HAVE_PTHREAD_CREATE)
	MESSAGE("libgeoip is missing on target. Skipping ${PROJECT_NAME} build.")
ENDIF (LIBGEOIP_FOUND AND CMAKE_HAVE_PTHREAD_CREATE)
