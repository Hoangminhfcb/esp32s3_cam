idf_component_register(
    SRCS "main.c" "camera.c" "wifi.c" "stream.c"
    INCLUDE_DIRS ".")

# Chọn board khi build mà không sửa config.h:
#   idf.py -DCAM_BOARD=XIAO build     hoặc   idf.py -DCAM_BOARD=OV5640 build
if(DEFINED CAM_BOARD)
    if(CAM_BOARD STREQUAL "XIAO")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE CAM_BOARD=2)
    elseif(CAM_BOARD STREQUAL "OV5640")
        target_compile_definitions(${COMPONENT_LIB} PRIVATE CAM_BOARD=1)
    else()
        message(FATAL_ERROR "CAM_BOARD phải là XIAO hoặc OV5640")
    endif()
endif()
