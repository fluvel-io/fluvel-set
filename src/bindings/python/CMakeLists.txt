# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy

# current binding python directory
set(PY_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(INTEROP_DIR         ${PY_DIR}/interop)

set(python_src

    module.cpp

    noise_py.cpp
    noise_py.hpp

    filtering_py.cpp
    filtering_py.hpp

    morpho_py.cpp
    morpho_py.hpp

    active_contour_py.cpp
    active_contour_py.hpp

    hausdorff_py.cpp
    hausdorff_py.hpp

    py_validation.cpp
    py_validation.hpp

    ${INTEROP_DIR}/py_array_conversion.cpp
    ${INTEROP_DIR}/py_array_conversion.hpp

    ${INTEROP_DIR}/contour_conversion.cpp
    ${INTEROP_DIR}/contour_conversion.hpp
)

pybind11_add_module(
    ${PYTHON_TARGET_NAME}
    ${python_src}
)

fluvel_configure_target(${PYTHON_TARGET_NAME})

target_link_libraries(
    ${PYTHON_TARGET_NAME}

    PRIVATE
        fluvel::image_processing
)

target_include_directories(${PYTHON_TARGET_NAME} PRIVATE
    ${INTEROP_DIR}
    ${PROJECT_SOURCE_DIR}/src/image_processing
)

# --- Build traceability (same information as the application) ---
# Priority: -DFLUVEL_GIT_COMMIT, then GITHUB_SHA (CI), then git, then "unknown".
if(NOT DEFINED FLUVEL_GIT_COMMIT OR FLUVEL_GIT_COMMIT STREQUAL "")
    if(DEFINED ENV{GITHUB_SHA} AND NOT "$ENV{GITHUB_SHA}" STREQUAL "")
        string(SUBSTRING "$ENV{GITHUB_SHA}" 0 7 FLUVEL_GIT_COMMIT)
    else()
        execute_process(
            COMMAND git rev-parse --short=7 HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE FLUVEL_GIT_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
endif()

if(NOT FLUVEL_GIT_COMMIT)
    set(FLUVEL_GIT_COMMIT "unknown")
endif()

message(STATUS "Python bindings commit: ${FLUVEL_GIT_COMMIT}")

target_compile_definitions(${PYTHON_TARGET_NAME} PRIVATE
    FLUVEL_VERSION="${PROJECT_VERSION}"
    FLUVEL_GIT_COMMIT="${FLUVEL_GIT_COMMIT}"
)

set_target_properties(
    ${PYTHON_TARGET_NAME}

    PROPERTIES

        LIBRARY_OUTPUT_DIRECTORY
            ${CMAKE_BINARY_DIR}/python

        OUTPUT_NAME
            "fluvel"
)

# Installed at the root of the wheel when built by scikit-build-core
# (pip wheel / cibuildwheel), so that "import fluvel" works.
if(SKBUILD)
    install(
        TARGETS ${PYTHON_TARGET_NAME}
        LIBRARY DESTINATION .
        RUNTIME DESTINATION .
    )
endif()
