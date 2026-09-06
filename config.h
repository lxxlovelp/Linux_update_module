#pragma once
#include <string>
#include <cstdint>

// 全局配置
const int PORT = 9000;

// 上传目录
const std::string UPLOAD_DIR = "./uploads/";

// 临时升级包
const std::string TEMP_PACKAGE_PATH = UPLOAD_DIR + "uploading.zip";

// 校验通过后保存的正式升级包
const std::string PACKAGE_PATH = UPLOAD_DIR + "current_upgrade_package.zip";

// 临时解压目录
const std::string EXTRACT_DIR = UPLOAD_DIR + "extracted";

// 当前设备信息
const std::string DEVICE_PRODUCT = "db11";
const std::string DEVICE_HARDWARE = "V1";

// 当前设备安全版本
const uint64_t DEVICE_SECURITY_VERSION = 10;