cmake_minimum_required(VERSION 3.1.1 FATAL_ERROR)

set(PROJECT_MAIN_PATH "jani")

set(LIB_PROJECT_NAME "jani_lib")
project(${LIB_PROJECT_NAME} VERSION 1.0.0 DESCRIPTION "blob")

set(WORKER_PROJECT_NAME "jani_worker")
project(${WORKER_PROJECT_NAME} VERSION 1.0.0 DESCRIPTION "blob")

set(RUNTIME_WORKER_SPAWNER_NAME "jani_worker_spawner")
project(${RUNTIME_WORKER_SPAWNER_NAME} VERSION 1.0.0 DESCRIPTION "blob")

set(RUNTIME_PROJECT_NAME "jani_runtime")
project(${RUNTIME_PROJECT_NAME} VERSION 1.0.0 DESCRIPTION "blob")

set(INSPECTOR_PROJECT_NAME "jani_inspector")
project(${INSPECTOR_PROJECT_NAME} VERSION 1.0.0 DESCRIPTION "blob")

add_definitions("-DCPP_SRC_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\"")
add_compile_definitions(_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)

############
# INCLUDES #
############
include(FetchContent)

if(WIN32)
	# Resource VersionInfo
	set(PROJECT_PRODUCT_NAME "jani")
	set(PROJECT_COMPANY_NAME "Rodrigo Holztrattner Reis")
endif()

################
# DEPENDENCIES #
################

macro(PrintFetchMessage target_string)
	string(CONCAT end_string "Fetching " ${target_string})
	message(STATUS "")
	message(STATUS "################################")
	message(STATUS "# ${end_string}")
	message(STATUS "################################")
	message(STATUS "")
endmacro()

# CTTI (compite time type information for C++)
FetchContent_Declare(
  ctti
  GIT_REPOSITORY https://github.com/RodrigoHolztrattner/ctti
)

# KCP (A Fast and Reliable ARQ Protocol)
FetchContent_Declare(
  kcp
  GIT_REPOSITORY https://github.com/RodrigoHolztrattner/kcp
)

# Nlohmann JSON (modern JSON for C++)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json
)

# Concurrent queue
FetchContent_Declare(
  concurrentqueue
  GIT_REPOSITORY https://github.com/cameron314/concurrentqueue#moodycamelconcurrentqueue
)

# IMGUI
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/RodrigoHolztrattner/imgui
  GIT_TAG        origin/docking-2020-05-12
)

# Magic enum
FetchContent_Declare(
  magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum
)

# Magic get
FetchContent_Declare(
  magic_get
  GIT_REPOSITORY https://github.com/RodrigoHolztrattner/magic_get.git
)

# Range V3
FetchContent_Declare(
  range_v3
  GIT_REPOSITORY https://github.com/ericniebler/range-v3
)

# Cereal
FetchContent_Declare(
  cereal
  GIT_REPOSITORY https://github.com/USCiLab/cereal
)

# EntityX (entity-component system)
FetchContent_Declare(
  entityx
  GIT_REPOSITORY https://github.com/RodrigoHolztrattner/entityx
)

# GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
)

# GLM (methematics library for graphics software)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm
)

# SPDLog (logging library)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog
)

# jobxx (Simple task and job scheduling library)
FetchContent_Declare(
  jobxx
  GIT_REPOSITORY https://github.com/RodrigoHolztrattner/jobxx
  GIT_TAG        origin/develop
)

# docopt (beautiful command-line interfaces)
#FetchContent_Declare(
#  docopt
#  GIT_REPOSITORY https://github.com/docopt/docopt.cpp
#)

FetchContent_GetProperties(ctti)
if(NOT ctti_POPULATED)
  PrintFetchMessage("CTTI")
  FetchContent_Populate(ctti)
  add_subdirectory(${ctti_SOURCE_DIR} ${ctti_BINARY_DIR})
endif()

FetchContent_GetProperties(kcp)
if(NOT kcp_POPULATED)
  PrintFetchMessage("KCP")
  FetchContent_Populate(kcp)
  add_subdirectory(${kcp_SOURCE_DIR} ${kcp_BINARY_DIR})
endif()

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  PrintFetchMessage("Nlohmann JSON")
  FetchContent_Populate(json)
endif()

FetchContent_GetProperties(concurrentqueue)
if(NOT concurrentqueue_POPULATED)
  PrintFetchMessage("moodycamel::ConcurrentQueue")
  FetchContent_Populate(concurrentqueue)
endif()

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  PrintFetchMessage("IMGUI")
  FetchContent_Populate(imgui)
  add_subdirectory(${imgui_SOURCE_DIR} ${imgui_BINARY_DIR})
endif()

FetchContent_GetProperties(magic_enum)
if(NOT magic_enum_POPULATED)
  PrintFetchMessage("magic_enum")
  FetchContent_Populate(magic_enum)
endif()

FetchContent_GetProperties(magic_get)
if(NOT magic_get_POPULATED)
  PrintFetchMessage("magic_get")
  FetchContent_Populate(magic_get)
endif()

FetchContent_GetProperties(range_v3)
if(NOT range_v3_POPULATED)
  PrintFetchMessage("range_v3")
  FetchContent_Populate(range_v3)
endif()

FetchContent_GetProperties(cereal)
if(NOT cereal_POPULATED)
  PrintFetchMessage("cereal")
  FetchContent_Populate(cereal)
endif()

FetchContent_GetProperties(entityx)
if(NOT entityx_POPULATED)
  PrintFetchMessage("EntityX")
  FetchContent_Populate(entityx)
  set(ENTITYX_BUILD_SHARED false CACHE BOOL "" FORCE)
  add_subdirectory(${entityx_SOURCE_DIR} ${entityx_BINARY_DIR})
endif()

FetchContent_GetProperties(glfw)
if(NOT glfw_POPULATED)
  PrintFetchMessage("GLFW")
  FetchContent_Populate(glfw)
  add_subdirectory(${glfw_SOURCE_DIR} ${glfw_BINARY_DIR})
endif()

FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
  PrintFetchMessage("GLM")
  FetchContent_Populate(glm)
endif()

FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
  PrintFetchMessage("SPDLOG")
  FetchContent_Populate(spdlog)
  add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()

FetchContent_GetProperties(jobxx)
if(NOT jobxx_POPULATED)
  PrintFetchMessage("jobxx")
  FetchContent_Populate(jobxx)
  add_subdirectory(${jobxx_SOURCE_DIR} ${jobxx_BINARY_DIR})
endif()

#FetchContent_GetProperties(docopt)
#if(NOT docopt_POPULATED)
#  PrintFetchMessage("docopt")
#  FetchContent_Populate(docopt)
#  add_subdirectory(${docopt_SOURCE_DIR} ${docopt_BINARY_DIR})
#endif()

###########
# PROJECT #
###########

SET(CMAKE_CXX_FLAGS "/Zi /sdl /MP /Oi /Gy /permissive- /GR- /TP /EHsc")
SET(CMAKE_CXX_FLAGS_DEBUG "/ZI /sdl /MP /JMC /permissive- /GR- /TP /EHsc /MDd /Od")



macro(retrieve_all_files_on_directory TARGET_FILE_OUTPUT TARGET_PATH)

  # Gather all files
  file (GLOB_RECURSE TARGET_SOURCE_FILES ${TARGET_PATH}/*.cpp ${TARGET_PATH}/*.cc) 
  file (GLOB_RECURSE TARGET_HEADER_FILES ${TARGET_PATH}/*.hpp ${TARGET_PATH}/*.h) 
  set (ALL_TARGET_FILES ${TARGET_SOURCE_FILES} ${TARGET_HEADER_FILES})

  # Setup the source groups
  foreach(FILE ${ALL_TARGET_FILES}) 

      # Get the directory of the source file
      get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

      # Remove common directory prefix to make the group
      string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_MAIN_PATH}" "" GROUP "${PARENT_DIR}")

      # Make sure we are using windows slashes
      string(REPLACE "/" "\\" GROUP "${GROUP}")

      source_group("${GROUP}" FILES "${FILE}")
    
  endforeach()

  set(${TARGET_FILE_OUTPUT} ${ALL_TARGET_FILES})
endmacro()

retrieve_all_files_on_directory(JANI_CORE_FILES "${PROJECT_MAIN_PATH}/core")

include(GNUInstallDirs)

add_definitions(-DUNICODE 
  -D_UNICODE)

set (COMMON_INCLUDE_DIRS 
${ctti_SOURCE_DIR}/include
${kcp_SOURCE_DIR}
${json_SOURCE_DIR}/single_include
${concurrentqueue_SOURCE_DIR}
${imgui_SOURCE_DIR}
${magic_enum_SOURCE_DIR}/include
${magic_get_SOURCE_DIR}/include
${range_v3_SOURCE_DIR}/include
${cereal_SOURCE_DIR}/include
${entityx_SOURCE_DIR}
${glfw_SOURCE_DIR}/include
${glm_SOURCE_DIR}
${spdlog_SOURCE_DIR}/include
${jobxx_SOURCE_DIR}/include)

###########
# LIBRARY #
###########

retrieve_all_files_on_directory(JANI_LIB_FILES "${PROJECT_MAIN_PATH}/library")

add_library(${LIB_PROJECT_NAME} STATIC ${JANI_CORE_FILES} ${JANI_LIB_FILES})

target_include_directories(${LIB_PROJECT_NAME} PUBLIC ${COMMON_INCLUDE_DIRS})
target_link_libraries(${LIB_PROJECT_NAME} kcp)
target_link_libraries(${LIB_PROJECT_NAME} entityx)
# target_link_libraries(${LIB_PROJECT_NAME} jobxx)

install(TARGETS ${LIB_PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  COMPONENT library)

set_target_properties(${LIB_PROJECT_NAME} PROPERTIES
  CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON)    
	
##########
# WORKER #
##########

SET(CMAKE_CXX_FLAGS "/Zi /sdl /MP /Oi /Gy /permissive- /GR- /TP /EHsc")
SET(CMAKE_CXX_FLAGS_DEBUG "/ZI /sdl /MP /JMC /permissive- /GR- /TP /EHsc /MDd /Od")

retrieve_all_files_on_directory(JANI_WORKER_FILES "${PROJECT_MAIN_PATH}/worker")

add_executable(${WORKER_PROJECT_NAME} ${JANI_CORE_FILES} ${JANI_WORKER_FILES})

target_include_directories(${WORKER_PROJECT_NAME} PUBLIC ${COMMON_INCLUDE_DIRS})
target_link_libraries(${WORKER_PROJECT_NAME} kcp)
target_link_libraries(${WORKER_PROJECT_NAME} entityx)
target_link_libraries(${WORKER_PROJECT_NAME} jobxx)

target_link_libraries(${WORKER_PROJECT_NAME} "Ws2_32.lib")
target_link_libraries(${WORKER_PROJECT_NAME} "Ws2_32.dll")

target_include_directories(${WORKER_PROJECT_NAME} PUBLIC ${PROJECT_MAIN_PATH}/core)

# Request C++ 17
set_target_properties(${WORKER_PROJECT_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)     

##################
# WORKER SPAWNER #
##################

SET(CMAKE_CXX_FLAGS "/Zi /sdl /MP /Oi /Gy /permissive- /GR- /TP /EHsc")
SET(CMAKE_CXX_FLAGS_DEBUG "/ZI /sdl /MP /JMC /permissive- /GR- /TP /EHsc /MDd /Od")

retrieve_all_files_on_directory(JANI_WORKER_FILES "${PROJECT_MAIN_PATH}/workerspawner")

add_executable(${RUNTIME_WORKER_SPAWNER_NAME} ${JANI_CORE_FILES} ${JANI_WORKER_FILES})

target_include_directories(${RUNTIME_WORKER_SPAWNER_NAME} PUBLIC ${COMMON_INCLUDE_DIRS})
target_link_libraries(${RUNTIME_WORKER_SPAWNER_NAME} kcp)
target_link_libraries(${RUNTIME_WORKER_SPAWNER_NAME} entityx)
target_link_libraries(${RUNTIME_WORKER_SPAWNER_NAME} jobxx)

target_link_libraries(${RUNTIME_WORKER_SPAWNER_NAME} "Ws2_32.lib")
target_link_libraries(${RUNTIME_WORKER_SPAWNER_NAME} "Ws2_32.dll")

target_include_directories(${RUNTIME_WORKER_SPAWNER_NAME} PUBLIC ${PROJECT_MAIN_PATH}/core)

# Request C++ 17
set_target_properties(${RUNTIME_WORKER_SPAWNER_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)  

###########
# RUNTIME #
###########

SET(CMAKE_CXX_FLAGS "/Zi /sdl /MP /Oi /Gy /permissive- /GR- /TP /EHsc")
SET(CMAKE_CXX_FLAGS_DEBUG "/ZI /sdl /MP /JMC /permissive- /GR- /TP /EHsc /MDd /Od")

retrieve_all_files_on_directory(JANI_RUNTIME_FILES "${PROJECT_MAIN_PATH}/runtime")

add_executable(${RUNTIME_PROJECT_NAME} ${JANI_CORE_FILES} ${JANI_RUNTIME_FILES})

target_include_directories(${RUNTIME_PROJECT_NAME} PUBLIC ${COMMON_INCLUDE_DIRS})
target_link_libraries(${RUNTIME_PROJECT_NAME} kcp)
target_link_libraries(${RUNTIME_PROJECT_NAME} entityx)
target_link_libraries(${RUNTIME_PROJECT_NAME} jobxx)

target_link_libraries(${RUNTIME_PROJECT_NAME} "Ws2_32.lib")
target_link_libraries(${RUNTIME_PROJECT_NAME} "Ws2_32.dll")

target_include_directories(${RUNTIME_PROJECT_NAME} PUBLIC ${PROJECT_MAIN_PATH}/core)

# Request C++ 17
set_target_properties(${RUNTIME_PROJECT_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)   

#############
# INSPECTOR #
#############

SET(CMAKE_CXX_FLAGS "/Zi /sdl /MP /Oi /Gy /permissive- /GR- /TP /EHsc")
SET(CMAKE_CXX_FLAGS_DEBUG "/ZI /sdl /MP /JMC /permissive- /GR- /TP /EHsc /MDd /Od")

retrieve_all_files_on_directory(JANI_WORKER_FILES "${PROJECT_MAIN_PATH}/inspector")

add_executable(${INSPECTOR_PROJECT_NAME} ${JANI_CORE_FILES} ${JANI_WORKER_FILES})

target_include_directories(${INSPECTOR_PROJECT_NAME} PUBLIC ${COMMON_INCLUDE_DIRS})
target_link_libraries(${INSPECTOR_PROJECT_NAME} kcp)
target_link_libraries(${INSPECTOR_PROJECT_NAME} entityx)
target_link_libraries(${INSPECTOR_PROJECT_NAME} jobxx)

target_link_libraries(${INSPECTOR_PROJECT_NAME} "Ws2_32.lib")
target_link_libraries(${INSPECTOR_PROJECT_NAME} "Ws2_32.dll")

target_link_libraries(${INSPECTOR_PROJECT_NAME} glfw)
target_link_libraries(${INSPECTOR_PROJECT_NAME} imgui)

target_include_directories(${INSPECTOR_PROJECT_NAME} PUBLIC ${PROJECT_MAIN_PATH}/core)

# Request C++ 17
set_target_properties(${INSPECTOR_PROJECT_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)   