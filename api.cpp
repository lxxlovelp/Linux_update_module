#include "api.h"
#include "config.h"
#include "package.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "httplib.h"

namespace fs = std::filesystem;

void setupUploadApi(httplib::Server& svr) {
    svr.Post("/api/upload_package", [](const httplib::Request& req, httplib::Response& res) {
        
        std::cout << "\n[上传] 收到上传请求" << std::endl;
        
        if (!req.form.has_file("file")) {
            res.status = 400;
            res.set_content("未找到有效的升级包", "text/plain; charset=utf-8");
            return;
        }

        const auto& file = req.form.get_file("file");
        if (file.filename.empty() || file.content.empty()) {
            res.status = 400;
            res.set_content("上传文件为空或文件名无效", "text/plain; charset=utf-8");
            return;
        }

        std::cout << "[上传] 原始文件名: " << file.filename << std::endl;
        std::cout << "[上传] 文件大小: " << file.content.size() << " bytes" << std::endl;

        std::ofstream ofs(TEMP_PACKAGE_PATH, std::ios::binary);
        if (!ofs.is_open()) {
            res.status = 500;
            res.set_content("无法保存临时升级包", "text/plain; charset=utf-8");
            return;
        }

        ofs.write(file.content.data(), static_cast<std::streamsize>(file.content.size()));
        if (!ofs.good()) {
            ofs.close();
            res.status = 500;
            res.set_content("升级包写入失败", "text/plain; charset=utf-8");
            return;
        }
        ofs.close();

        std::cout << "[上传] 临时文件: " << TEMP_PACKAGE_PATH << std::endl;

        bool valid = validateUpgradePackage(TEMP_PACKAGE_PATH);
        if (!valid) {
            std::cerr << "[上传]  升级包验证失败" << std::endl;
            // 删除临时文件
            std::remove(TEMP_PACKAGE_PATH.c_str());
            res.status = 400;
            res.set_content("升级包校验失败", "text/plain; charset=utf-8");
            return;
        }

        std::cout << "[上传] 升级包验证成功" << std::endl;

        std::error_code ec;
        fs::remove(PACKAGE_PATH, ec);
        fs::rename(TEMP_PACKAGE_PATH, PACKAGE_PATH, ec);

        if (ec) {
            std::cerr << "[上传]  保存正式升级包失败: " << ec.message() << std::endl;
            std::remove(TEMP_PACKAGE_PATH.c_str());
            res.status = 500;
            res.set_content("升级包保存失败", "text/plain; charset=utf-8");
            return;
        }

        std::cout << "[上传]  升级包保存成功" << std::endl;
        std::cout << "[上传] 正式路径: " << PACKAGE_PATH << std::endl;

        res.status = 200;
        res.set_content("升级包上传并校验成功", "text/plain; charset=utf-8");
    });
}


void setupDownloadApi(httplib::Server& svr) {
    svr.Get("/api/download_package", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[下载] 收到下载请求" << std::endl;

        std::ifstream ifs(PACKAGE_PATH, std::ios::binary | std::ios::ate);//std::ios::ate 打开文件后，直接把读写位置放到文件末尾。
        if (!ifs.is_open()) {
            res.status = 404;
            res.set_content("服务器上当前没有任何升级包", "text/plain; charset=utf-8");
            std::cout << "[下载]  升级包不存在" << std::endl;
            return;
        }

        std::streamsize size = ifs.tellg();//获取文件当前读写位置，并把它当作文件大小。streamsize文件流相关的大小/位置
        if (size <= 0) {
            ifs.close();
            res.status = 500;
            res.set_content("升级包文件损坏或为空", "text/plain; charset=utf-8");
            return;
        }

        ifs.seekg(0, std::ios::beg);
        /*
        把文件的读取位置移动到文件开头。
        0：偏移 0
        std::ios::beg：以文件开头为基
        */
        std::string content(static_cast<size_t>(size), '\0');
        if (!ifs.read(&content[0], size)) {
            ifs.close();
            res.status = 500;
            res.set_content("读取升级包失败", "text/plain; charset=utf-8");
            return;
        }
        ifs.close();

        std::cout << "[下载] 正在传送升级包: " << size << " bytes" << std::endl;
        res.set_header("Content-Disposition", "attachment; filename=\"system_update_package.zip\"");
        res.set_content(content, "application/octet-stream");
        res.status = 200;
    });
}