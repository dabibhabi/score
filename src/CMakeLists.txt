# ---------------------------------------------------------------------------
# Core shared library: score_core
# Compiles C++23 sources from core/, statistics/, metrics/, db/, net/, and
# the extern "C" API implemented in c_api.cpp.
# ---------------------------------------------------------------------------

set(SCORE_CORE_CPP_SOURCES
    core/exceptions.cpp
    core/types.cpp
    statistics/descriptive.cpp
    statistics/normal_pdf.cpp
    statistics/cdf.cpp
    statistics/risk.cpp
    metrics/stock_metrics.cpp
    db/date_codec.cpp
    db/column_file.cpp
    db/symbol_dir.cpp
    db/column_view.cpp
    db/column_store_v1.cpp
    net/json_codec.cpp
    net/connection.cpp
    net/server.cpp
    net/event_loop_blocking.cpp
    c_api.cpp
)

add_library(score_core SHARED
    ${SCORE_CORE_CPP_SOURCES}
)

target_include_directories(score_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link nlohmann/json (header-only; this propagates the include path)
find_package(Threads REQUIRED)
target_link_libraries(score_core
    PUBLIC
        nlohmann_json::nlohmann_json
        Threads::Threads
)

# Force explicit standards on this target (in case the user overrides globally)
set_target_properties(score_core PROPERTIES
    CXX_STANDARD          23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
    VERSION               ${PROJECT_VERSION}
    SOVERSION             ${PROJECT_VERSION_MAJOR}
)

# Export symbols on Windows (no-op on Unix)
include(GenerateExportHeader)
generate_export_header(score_core
    BASE_NAME SCORE
    EXPORT_FILE_NAME ${CMAKE_BINARY_DIR}/score_export.h
)
target_include_directories(score_core PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
