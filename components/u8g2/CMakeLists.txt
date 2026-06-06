file(GLOB_RECURSE srcs "csrc/*.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "csrc")