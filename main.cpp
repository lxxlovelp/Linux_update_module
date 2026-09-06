#include "config.h"
#include "utils.h"
#include "api.h"
#include "/home/xingxinliao/update/httplib.h"
#include <iostream>
#include "httplib.h"

int main() {
    create_upload_directory();

    std::cout << "======================================\n";
    std::cout << " C++ 升级包管理后端\n";
    std::cout << "======================================\n";

    httplib::Server svr;

    setupUploadApi(svr);
    setupDownloadApi(svr);

    std::cout << "监听 IP: 127.0.0.1\n";
    std::cout << "监听端口: " << PORT << "\n";
    std::cout << "======================================\n";

    svr.listen("127.0.0.1", PORT);

    return 0;
}