cmake_minimum_required(VERSION 3.16.3)
project(fhe_vector_db CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_STATIC "Link OpenFHE statically" OFF)

set(OPENFHE_LOCAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/openfhe-development")
set(OPENFHE_LOCAL_BUILD "${OPENFHE_LOCAL_ROOT}/build")

find_package(OpenFHE CONFIG QUIET)

set(_use_local OFF)
if(OpenFHE_FOUND AND EXISTS "${OpenFHE_INCLUDE}/pke/openfhe.h")
    message(STATUS "Using installed OpenFHE ${BASE_OPENFHE_VERSION} at ${OpenFHE_INCLUDE}")
    set(CMAKE_CXX_FLAGS "${OpenFHE_CXX_FLAGS}")
    include_directories(
        ${OPENMP_INCLUDES}
        ${OpenFHE_INCLUDE}
        ${OpenFHE_INCLUDE}/third-party/include
        ${OpenFHE_INCLUDE}/core
        ${OpenFHE_INCLUDE}/pke
        ${OpenFHE_INCLUDE}/binfhe
    )
    if(UNIX AND NOT APPLE)
        add_link_options(-Wl,--no-as-needed)
    endif()
    link_directories(${OpenFHE_LIBDIR} ${OPENMP_LIBRARIES})
    if(BUILD_STATIC)
        set(CMAKE_EXE_LINKER_FLAGS "${OpenFHE_EXE_LINKER_FLAGS} -static")
        set(_openfhe_libs ${OpenFHE_STATIC_LIBRARIES})
    else()
        set(CMAKE_EXE_LINKER_FLAGS "${OpenFHE_EXE_LINKER_FLAGS}")
        set(_openfhe_libs ${OpenFHE_SHARED_LIBRARIES})
    endif()
    set(_openfhe_rpath "${OpenFHE_LIBDIR}")
else()
    if(NOT EXISTS "${OPENFHE_LOCAL_BUILD}/lib")
        message(FATAL_ERROR
            "OpenFHE not found. Install it, or build the checkout:\n"
            "  cmake -S ${OPENFHE_LOCAL_ROOT} -B ${OPENFHE_LOCAL_BUILD} && cmake --build ${OPENFHE_LOCAL_BUILD}")
    endif()
    message(STATUS "Using local OpenFHE build at ${OPENFHE_LOCAL_BUILD}")
    set(_use_local ON)
    add_compile_definitions(MATHBACKEND=4 OPENFHE_VERSION=1.5.1)
    include_directories(
        ${OPENFHE_LOCAL_ROOT}/src/pke/include
        ${OPENFHE_LOCAL_ROOT}/src/core/include
        ${OPENFHE_LOCAL_ROOT}/src/binfhe/include
        ${OPENFHE_LOCAL_ROOT}/third-party/cereal/include
        ${OPENFHE_LOCAL_BUILD}/src/core
    )
    link_directories(${OPENFHE_LOCAL_BUILD}/lib)
    if(BUILD_STATIC)
        set(_openfhe_libs OPENFHEpke_static OPENFHEcore_static OPENFHEbinfhe_static)
    else()
        set(_openfhe_libs OPENFHEpke OPENFHEcore OPENFHEbinfhe)
    endif()
    set(_openfhe_rpath "${OPENFHE_LOCAL_BUILD}/lib")

    if(APPLE)
        foreach(_omp_root /usr/local/opt/libomp /opt/homebrew/opt/libomp)
            if(EXISTS "${_omp_root}/include")
                include_directories(${_omp_root}/include)
                link_directories(${_omp_root}/lib)
                add_compile_options(-Xpreprocessor -fopenmp)
                list(APPEND _openfhe_libs omp)
                break()
            endif()
        endforeach()
    else()
        find_package(OpenMP)
        if(OpenMP_CXX_FOUND)
            list(APPEND _openfhe_libs OpenMP::OpenMP_CXX)
        endif()
    endif()
endif()

add_executable(fhe_vector_db fhe_vector_db.cpp)
target_include_directories(fhe_vector_db PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(fhe_vector_db PRIVATE FHE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fhe_vector_db PRIVATE ${_openfhe_libs})
set_target_properties(fhe_vector_db PROPERTIES
    BUILD_RPATH "${_openfhe_rpath}"
    INSTALL_RPATH "${_openfhe_rpath}"
)
