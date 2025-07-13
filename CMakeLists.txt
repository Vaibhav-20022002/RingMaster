# Set the minimum required version of CMake required
cmake_minimum_required(VERSION 3.10)

# Define the project name
project(RingMaster CXX)

# Set C++ stndard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# * STEP:1 - Detect Cache Line Size of the system :
message(STATUS "Attempting to detect cache line size by compiling & running a utility...")

try_run(
  # These variables will hold the results of the operation
  RUN_RESULT_CODE    ## Exit code of the utility binary
  COMPILE_RESULT     ## SUCCESS (0) or FAILURE(non-zero)

  # The directory where the test binary will be built. Must be an absolute path.
  "${CMAKE_BINARY_DIR}/cacheLineTest"

  # The source file to compile and run. Must be an absolute path.
  "${CMAKE_SOURCE_DIR}/tools/cacheLineSize.cc"

  # This option tells try_run to store the standard output from the program
  # in the specified variable.
  RUN_OUTPUT_VARIABLE DETECTED_CACHE_LINE_SIZE
)

message(STATUS "value of current source directory: ${CMAKE_SOURCE_DIR}.")

# * Step:2 - Process the result :
if(COMPILE_RESULT AND "${RUN_RESULT_CODE}" STREQUAL "0")
  # The output might have leading/trailing whitespace, so we clean it up.
  string(STRIP "${DETECTED_CACHE_LINE_SIZE}" DETECTED_CACHE_LINE_SIZE)
  message(STATUS "Sucessfully detected cache line size: ${DETECTED_CACHE_LINE_SIZE} bytes.")
else()
  # If the process failed, fall back to safe default and warn
  set(DETECTED_CACHE_LINE_SIZE 64) # Common and safe default
  message((WARNING "Couldn't determine cache line size automatically. Falling back to a default of ${DETECTED_CACHE_LINE_SIZE} bytes. Performance may not be optimal."))
  if(NOT COMPILE_RESULT)
    message(WARNING "Reason: The utility failed to compile.")
  else()
    message(WARNING "Reason: The utility program ran but exited with the code: ${RUN_CAHE_LINE_SIZE}.")
  endif()
endif()

# * Step:3 - Build the main application :
add_executable(exe test.cpp)

# * Step:4 - Link RingMaster.hh for the buffer integration :
target_include_directories(exe PRIVATE ${CMAKE_SOURCE_DIR})

# * Step:5 - Pass the Captured Value as a Compile time constant :
target_compile_definitions(exe PRIVATE
  CACHE_LINE_SIZE=${DETECTED_CACHE_LINE_SIZE}
)
