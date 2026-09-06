#include "utils.h"
#include "config.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include "httplib.h"
#include <openssl/pem.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <openssl/evp.h>

namespace fs = std::filesystem;

void create_upload_directory() {
#ifdef _WIN32
    _mkdir(UPLOAD_DIR.c_str());
#else
    mkdir(UPLOAD_DIR.c_str(), 0755);
#endif
}

std::string calculateSha256(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[SHA256] 无法打开文件: " << filePath << std::endl;
        return "";
    }
    //创建一个用于进行哈希计算的上下文对象，然后让 ctx 指向它。
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        std::cerr << "[SHA256] EVP_MD_CTX_new 失败" << std::endl;
        return "";
    }
    //告诉 OpenSSL：这个 ctx 接下来要用什么哈希算法。
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    const size_t BUFFER_SIZE = 1024 * 1024;
    std::vector<char> buffer(BUFFER_SIZE);

    while (file.good()) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        //获取刚才 file.read() 实际读了多少字
        std::streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            //把刚刚读取的文件数据交给 SHA256 计算。
            if (EVP_DigestUpdate(ctx, buffer.data(), static_cast<size_t>(bytesRead)) != 1) {
                EVP_MD_CTX_free(ctx);
                return "";
            }
        }
    }
    //准备存放最终的 SHA256 结果。
    unsigned char hash[EVP_MAX_MD_SIZE];
    //hashLength：记录最终哈希值有多长。
    unsigned int hashLength = 0;
    //SHA256 已经算完了，把最终结果放进 hash。
    if (EVP_DigestFinal_ex(ctx, hash, &hashLength) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    EVP_MD_CTX_free(ctx);
    //创建一个字符串流，用来拼接/生成字符串。
    std::stringstream ss;
    for (unsigned int i = 0; i < hashLength; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        /*
        std::hex → 转十六进制
        setw(2) → 固定 2 位
        setfill('0') → 不足补 0
        static_cast 就是 C++ 的一种类型转换，hash[i] 转换成 int 类型。
        */
    }
    return ss.str();
}

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { 
                return static_cast<char>(std::tolower(c)); });
    return str;
}

bool verifyFileSha256(const std::string& filePath, const std::string& expectedSha256) {
    std::cout << "[SHA256] 正在计算: " << filePath << std::endl;
    std::string actualSha256 = calculateSha256(filePath);
    if (actualSha256.empty()) {
        std::cerr << "[SHA256] 计算失败" << std::endl;
        return false;
    }

    std::cout << "[SHA256] Expected: " << expectedSha256 << std::endl;
    std::cout << "[SHA256] Actual  : " << actualSha256 << std::endl;

    if (toLower(actualSha256) != toLower(expectedSha256)) {
        std::cerr << "[SHA256] 校验失败: " << filePath << std::endl;
        std::cerr << "actual   = [" << actualSha256 << "] len=" << actualSha256.length() << std::endl;
        std::cerr << "expected = [" << expectedSha256 << "] len=" << expectedSha256.length() << std::endl;
        return false;
    }

    std::cout << "[SHA256]校验成功: " << filePath << std::endl;
    return true;
}

bool extractZip(const std::string& zipPath, const std::string& extractDir) {
    std::error_code ec;//创建一个用来保存错误信息的变量。
    if (fs::exists(extractDir)) {
        fs::remove_all(extractDir, ec);// 删除 extractDir 目录以及里面的所有文件，并把可能出现的错误记录到 ec。
        if (ec) {
            std::cerr << "[解压] 删除旧目录失败: " << ec.message() << std::endl;
            return false;
        }
    }

    fs::create_directories(extractDir, ec);
    if (ec) {
        std::cerr << "[解压] 创建目录失败: " << ec.message() << std::endl;
        return false;
    }

    std::cout << "[解压] " << zipPath << std::endl;
    std::string command = "unzip -q -o \"" + zipPath + "\" -d \"" + extractDir + "\"";
    int result = std::system(command.c_str());//操作系统执行 command 这个字符串里的命令

    if (result != 0) {
        std::cerr << "[解压]  unzip 失败" << std::endl;
        return false;
    }

    std::cout << "[解压]  解压成功" << std::endl;
    return true;
}

// 读取整个文件内容到内存（适用于签名文件等小文件）
std::vector<unsigned char> readFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<unsigned char> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
}

// 核心：验证签名函数
bool verifySignature(const std::string& dataPath, 
                     const std::string& sigPath, 
                     const std::string& pubKeyPath) 
{
    std::cout << "[签名校验] 开始校验: " << dataPath << std::endl;

    // 1. 读取公钥文件 (PEM 格式)
    FILE* pubKeyFile = fopen(pubKeyPath.c_str(), "rb");
    /*
    "r" = read，只读
    "b" = binary，二进制模式
    */
    if (!pubKeyFile) {
        std::cerr << "[签名校验] 找不到公钥文件: " << pubKeyPath << std::endl;
        return false;
    }
    // 解析公钥
    EVP_PKEY* pubKey = PEM_read_PUBKEY(pubKeyFile, nullptr, nullptr, nullptr);
    //PEM_read_PUBKEY()函数用于从文件中读取 PEM 格式的公钥，并返回一个 EVP_PKEY 结构体指针。
    fclose(pubKeyFile);
    if (!pubKey) {
        std::cerr << "[签名校验] 解析公钥失败" << std::endl;
        return false;
    }

    // 2. 读取签名文件 (二进制数据)
    std::vector<unsigned char> signature = readFile(sigPath);
    if (signature.empty()) {
        std::cerr << "[签名校验] 读取签名文件失败: " << sigPath << std::endl;
        EVP_PKEY_free(pubKey);
        return false;
    }

    // 3. 初始化验证上下文
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pubKey);
        return false;
    }

    // 这里指定使用 SHA-256 算法来进行签名校验
    if (EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pubKey) != 1) {
        std::cerr << "[签名校验] 验证环境初始化失败" << std::endl;
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pubKey);
        return false;
    }

    // 4. 分块读取待验证的数据文件 (manifest.json)，并输入到验证器中
    std::ifstream dataFile(dataPath, std::ios::binary);
    if (!dataFile.is_open()) {
        std::cerr << "[签名校验] 无法打开数据文件: " << dataPath << std::endl;
        EVP_MD_CTX_free(ctx);
        EVP_PKEY_free(pubKey);
        return false;
    }

    const size_t BUFFER_SIZE = 1024 * 1024; // 1MB 缓冲
    std::vector<char> buffer(BUFFER_SIZE);
    while (dataFile.good()) {
        dataFile.read(buffer.data(), buffer.size());
        std::streamsize bytesRead = dataFile.gcount();
        if (bytesRead > 0) {
            // 将数据“喂”给验证器
            if (EVP_DigestVerifyUpdate(ctx, buffer.data(), bytesRead) != 1) {
                std::cerr << "[签名校验] 数据输入失败" << std::endl;
                EVP_MD_CTX_free(ctx);
                EVP_PKEY_free(pubKey);
                return false;
            }
        }
    }
    /*
    当你调用 EVP_DigestVerifyUpdate 时，大管家知道你最终要用 Ed25519。所以它拿到你读入的文件数据后，不会去算普通的 SHA-256，
    而是直接把这段数据送进 Ed25519 专属的数学状态机里（同时把公钥也混进去），从而完美避开崩溃。
    
    */

    // 5. 进行最终校验
    int verifyResult = EVP_DigestVerifyFinal(ctx, signature.data(), signature.size());

    // 6. 清理内存 (C 语言风格的库需要手动释放)
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pubKey);

    if (verifyResult == 1) {
        std::cout << "[签名校验] 签名验证通过，文件是官方发布的！" << std::endl;
        return true;
    } else {
        std::cerr << "[签名校验] 签名验证失败，文件可能被篡改或签名不匹配！" << std::endl;
        return false;
    }
}