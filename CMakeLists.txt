# 指定 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 定义项目名称和使用的语言
project(UpdateServer LANGUAGES CXX)

# 设置 C++ 标准为 C++17 (std::filesystem 需要)
set(CMAKE_CXX_STANDARD 17)
#必须使用指定的 C++ 标准，否则 CMake 配置失败。
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ==========================================
# 查找依赖包
# ==========================================

# 1. 查找多线程库 (httplib 需要 pthread)
find_package(Threads REQUIRED)

# 2. 查找 OpenSSL 库 (SHA256 计算需要 crypto)
find_package(OpenSSL REQUIRED)

# ==========================================
# 编译目标
# ==========================================

# 收集所有的源文件
set(SOURCES 
    main.cpp
    utils.cpp
    package.cpp
    api.cpp
)

# 生成可执行文件 update_server
add_executable(update_server ${SOURCES})

# ==========================================
# 链接库和包含目录
# ==========================================

# 链接所需的第三方库，这些库只给 update_server 自己使用，不传递给链接它的其他 target。
target_link_libraries(update_server 
    PRIVATE 
    Threads::Threads 
    OpenSSL::SSL 
    OpenSSL::Crypto
)

# （可选）如果你的 json.hpp 没有放在系统默认路径，可以在这里添加它的搜索路径
# target_include_directories(update_server PRIVATE "/path/to/your/nlohmann_json/include")