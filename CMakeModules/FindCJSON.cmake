include (LibFindMacros)

find_path(CJSON_INCLUDE_DIR NAMES cjson/cJSON.h)
find_library(CJSON_LIBRARY NAMES cjson)

set(CJSON_PROCESS_INCLUDES CJSON_INCLUDE_DIR)
set(CJSON_PROCESS_LIBS CJSON_LIBRARY)

libfind_process(CJSON)
