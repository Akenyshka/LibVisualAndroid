INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
  ${LIBVISUAL_INCLUDE_DIRS}
)

LINK_DIRECTORIES(
  ${LIBVISUAL_LIBRARY_DIRS}
)

SET(input_debug_SOURCES
  input_debug.c
)

ADD_LIBRARY(input_debug MODULE ${input_debug_SOURCES})
#-avoid-version

TARGET_LINK_LIBRARIES(input_debug
  ${LIBVISUAL_LIBRARIES}
)

INSTALL(TARGETS input_debug LIBRARY DESTINATION ${LV_INPUT_PLUGIN_DIR})
