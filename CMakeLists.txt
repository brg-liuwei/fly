CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT (FLY)

INCLUDE_DIRECTORIES (/usr/local/include /usr/local/fcgi/include /usr/local/mxml/include/)

INCLUDE (CheckFunctionExists)
CHECK_FUNCTION_EXISTS (epoll FY_HAVE_EPOLL)
CHECK_FUNCTION_EXISTS (kqueue FY_HAVE_KQUEUE)

INCLUDE (CheckIncludeFiles)
CHECK_INCLUDE_FILES (malloc.h FY_HAVE_MALLOC_H)
CHECK_INCLUDE_FILES (stdlib.h FY_HAVE_STDLIB_H)
CHECK_INCLUDE_FILES(sys/types.h FY_HAVE_SYS_TYPES_H)

CONFIGURE_FILE (fy_config.h.in src/fy_config.h)
ADD_SUBDIRECTORY (src bin)
