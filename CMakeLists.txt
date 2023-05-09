cmake_minimum_required(VERSION 3.1)

# Set the project name
project(deviceLiveLinkFace)

# Find the MotionBuilder SDK
# find_package(MotionBuilder REQUIRED)

set(MotionBuilder_INCLUDE_DIRS "C:/Program Files/Autodesk/MotionBuilder 2019/OpenRealitySDK/include")
set(MotionBuilder_LIBRARIES "C:/Program Files/Autodesk/MotionBuilder 2019/OpenRealitySDK/lib/x64/fbsdk.lib")


# Set the source files for the plugin
set(SRC_FILES
    device_facecap.cxx
    device_facecap_device.cxx
    device_facecap_device.h
    device_facecap_hardware.cxx
    device_facecap_hardware.h
    device_facecap_layout.cxx
    device_facecap_layout.h
)

# Create the plugin target
add_library(deviceLiveLinkFace SHARED ${SRC_FILES})

# Set the target properties
target_include_directories(deviceLiveLinkFace PRIVATE ${MotionBuilder_INCLUDE_DIRS})
target_link_libraries(deviceLiveLinkFace PRIVATE ${MotionBuilder_LIBRARIES} ws2_32)

# Set the target output directory
set_target_properties(deviceLiveLinkFace PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Plugins
)