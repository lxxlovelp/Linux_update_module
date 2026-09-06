#pragma once
#include <string>
#include <nlohmann/json.hpp>

// 内部使用的函数也可以声明在这里或者 cpp 中
bool loadManifest(const std::string& manifestPath, nlohmann::json& manifest);
bool verifyManifestInfo(const nlohmann::json& manifest);
bool verifyPackageFiles(const std::string& extractDir, const nlohmann::json& manifest);

// 对外核心接口
bool validateUpgradePackage(const std::string& packagePath);