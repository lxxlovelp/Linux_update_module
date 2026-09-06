#include "package.h"
#include "config.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "httplib.h"

namespace fs = std::filesystem;
using json = nlohmann::json;//nlohmann::json 起一个别名，叫 json。

bool loadManifest(const std::string& manifestPath, json& manifest) {
    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        std::cerr << "[Manifest] 找不到: " << manifestPath << std::endl;
        return false;
    }
    try {
        file >> manifest;
    } catch (const std::exception& e) {
        std::cerr << "[Manifest] JSON 解析失败: " << e.what() << std::endl;
        return false;
        //e.what()e.what()获取具体的错误信息。

    }
    return true;
}

bool verifyManifestInfo(const json& manifest) {
    if (!manifest.contains("product") || !manifest["product"].is_string()) {
        std::cerr << "[检查] manifest 缺少 product" << std::endl;
        return false;
    }

    std::string packageProduct = manifest["product"].get<std::string>();//把 JSON 里的值取出来，并转换成 std::string。
    std::cout << "[检查] Package product: " << packageProduct << std::endl;
    std::cout << "[检查] Device product : " << DEVICE_PRODUCT << std::endl;

    if (packageProduct != DEVICE_PRODUCT) {
        std::cerr << "[检查]  产品型号不匹配" << std::endl;
        return false;
    }

    if (!manifest.contains("version") || !manifest["version"].is_string()) {
        std::cerr << "[检查] manifest 缺少 version" << std::endl;
        return false;
    }

    std::string version = manifest["version"].get<std::string>();
    std::cout << "[检查] Upgrade version: " << version << std::endl;

    if (!manifest.contains("security_version") || !manifest["security_version"].is_number_unsigned()) {
        std::cerr << "[检查] manifest 缺少 security_version" << std::endl;
        return false;
    }

    uint64_t securityVersion = manifest["security_version"].get<uint64_t>();//把 JSON 中的值取出来，并转换成 uint64_t 类型。
    std::cout << "[检查] Package security version: " << securityVersion << std::endl;
    std::cout << "[检查] Device security version : " << DEVICE_SECURITY_VERSION << std::endl;

    if (securityVersion < DEVICE_SECURITY_VERSION) {
        std::cerr << "[检查]  security_version 太低" << std::endl;
        return false;
    }

    return true;
}

bool verifyPackageFiles(const std::string& extractDir, const json& manifest) {
    if (!manifest.contains("files") || !manifest["files"].is_object()) {//断这个 JSON 值是不是一个 JSON 对象 {}。
        std::cerr << "[文件] manifest 中没有 files" << std::endl;
        return false;
    }

    const auto& files = manifest["files"];//把 JSON 里的 files 值取出来，并转换成 const auto& 类型。
    for (auto& [fileName, fileInfo] : files.items()) {
        //files.items() 每次拿出一个：
        std::cout << "\n[文件] 检查: " << fileName << std::endl;

        fs::path relativePath(fileName);
        if (relativePath.is_absolute() || fileName.find("..") != std::string::npos) {
            // .is_absolute判断是不是绝对路径,ind() 找不到时返回,std::string::npos
            std::cerr << "[文件]  非法文件路径: " << fileName << std::endl;
            return false;
        }

        if (!fileInfo.contains("sha256") || !fileInfo["sha256"].is_string()) {
            std::cerr << "[文件] 缺少 sha256: " << fileName << std::endl;
            return false;
        }

        std::string expectedSha256 = fileInfo["sha256"].get<std::string>();//从 JSON 中取出这个值，并转换成 std::string。
        fs::path fullPath = fs::path(extractDir) / relativePath;//解压目录和文件的相对路径拼起来，得到文件的完整路径

        if (!fs::exists(fullPath) || !fs::is_regular_file(fullPath)) {
            std::cerr << "[文件] 文件不存在: " << fullPath << std::endl;
            return false;
        }

        if (!verifyFileSha256(fullPath.string(), expectedSha256)) {
            return false;
        }
  
    }
           return true;
   
 }

bool validateUpgradePackage(const std::string& packagePath) {
    std::cout << "\n======================================" << std::endl;
    std::cout << "[升级包] 开始验证" << std::endl;
    std::cout << "======================================" << std::endl;

    if (!fs::exists(packagePath)) {
        std::cerr << "[升级包]  ZIP 不存在" << std::endl;
        return false;
    }
    //把 packagePath 指定的 ZIP 包，解压到 EXTRACT_DIR 目录。
    if (!extractZip(packagePath, EXTRACT_DIR)) {
        return false;
    }
    
    std::string manifestPath = EXTRACT_DIR + "/update/db11_2026_09_01_dv11_manifest.json";
    // std::cout << "packagePath = " << packagePath << std::endl;
    // std::cout << "manifestPath = " << manifestPath << std::endl;
    std::string sigPath = EXTRACT_DIR + "/update/manifest.sig";
    std::string pubKeyPath = "/home/xingxinliao/update/config/public_key.pem"; // 设备内置的公钥路径

    //先验证签名！
    if (!verifySignature(manifestPath, sigPath, pubKeyPath)) {
        std::cerr << "[升级包]  签名非法，拒绝升级！" << std::endl;
        return false;
    }
    
    json manifest;
    if (!loadManifest(manifestPath, manifest)) {
        return false;
    }
    if (!verifyManifestInfo(manifest)) {
        return false;
    }
    if (!verifyPackageFiles(EXTRACT_DIR, manifest)) {
        return false;
    }

    std::cout << "\n======================================" << std::endl;
    std::cout << "[升级包]所有校验通过" << std::endl;
    std::cout << "======================================\n" << std::endl;

    return true;
}