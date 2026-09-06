#pragma once
#include <string>
#include <vector>

void create_upload_directory();
std::string calculateSha256(const std::string& filePath);
std::string toLower(std::string str);
bool verifyFileSha256(const std::string& filePath, const std::string& expectedSha256);
bool extractZip(const std::string& zipPath, const std::string& extractDir);
bool verifySignature(const std::string& dataPath, const std::string& sigPath,const std::string& pubKeyPath);
std::vector<unsigned char> readFile(const std::string& filePath);