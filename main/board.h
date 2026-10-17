#pragma once
/* Chọn file chân cắm theo CAM_BOARD trong config.h. Không cần sửa file này. */
#include "config.h"

#if CAM_BOARD == BOARD_ESP32S3_OV5640
#include "boards/board_esp32s3_ov5640.h"
#elif CAM_BOARD == BOARD_XIAO_ESP32S3_SENSE
#include "boards/board_xiao_esp32s3_sense.h"
#else
#error "CAM_BOARD không hợp lệ — xem main/config.h"
#endif
