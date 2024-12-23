#if defined(_WIN32) || defined(_WIN64) // Windows Platfom
#define NOMINMAX
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <cctype>
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <cstdlib> // for system
using namespace std;

struct file {
    string fullfileName;
    string fileWithDir;
    string extensionName;
};

// 获取当前程序所在目录 (跨平台)
std::string get_executable_dir() {
#if defined(_WIN32) || defined(_WIN64)
    // Windows: 使用 GetModuleFileName 获取可执行文件路径
    char buffer[MAX_PATH];
    if (GetModuleFileNameA(NULL, buffer, MAX_PATH) == 0) {
        throw runtime_error("无法获取可执行文件路径");
    }
    return std::filesystem::path(buffer).parent_path().string();
#else
    // macOS/Linux 平台: 使用 readlink 获取可执行文件路径
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len == -1) {
        // Fallback，如果 readlink 失败，使用 argv[0]
        string exec_path = __FILE__;
        size_t pos = exec_path.find_last_of("/\\");
        return exec_path.substr(0, pos);  // 提取目录部分
    }
    buffer[len] = '\0';
    string exec_path(buffer);
    size_t pos = exec_path.find_last_of("/\\");
    return exec_path.substr(0, pos);  // 提取目录部分
#endif
}

// 更改工作目录为可执行文件所在目录
void change_working_directory_to_executable() {
    string execDir = get_executable_dir();
    filesystem::current_path(execDir);
}

// 转义路径中的空格
string escape_path(const string& path) {
#ifdef _WIN32
    // Windows 平台：空格用 ^ 转义
    string escaped_path;
    for (char c : path) {
        if (c == ' ') {
            escaped_path += '^';
        }
        escaped_path += c;
    }
    return escaped_path;
#else
    // macOS/Linux 平台：空格用 \ 转义
    string escaped_path;
    for (char c : path) {
        // 在 macOS/Linux 平台，确保正确转义空格和其他特殊字符
        if (c == ' ' || c == '~') {
            escaped_path += '\\';
        }
        escaped_path += c;
    }
    return escaped_path;
#endif
}

void process_image(const string& executable, const file& image, const int& noise_level, const int& scale_to, const bool enableTTA) {
    #if defined (_WIN32) || defined (_WIN64)
        string command = executable + " -i \"" + image.fullfileName + "\" -o \"" + image.fileWithDir + "." + image.extensionName + "\" -n " + to_string(noise_level) + " -s " + to_string(scale_to);
    #else
    // 转义路径中的空格
    string input_file = escape_path(image.fullfileName);
    string output_file = escape_path(image.fileWithDir + "-2." + image.extensionName);

    // 构建命令行字符串
    string command = executable + " -i " + input_file + " -o " + output_file + " -n " + to_string(noise_level) + " -s " + to_string(scale_to);
    #endif
    if (enableTTA) {
        command += " -x";
    }
    // 执行命令
    cout << endl << command << endl;
    int ret = system(command.c_str());

    // 检查命令执行结果
    if (ret != 0) {
        cout << image.fullfileName << " 处理失败" << endl;
        throw runtime_error(image.fullfileName);
    }
    else {
        cout << image.fullfileName << " 处理成功" << endl;
    }
}

int main() {
    change_working_directory_to_executable();

    filesystem::path current_path = std::filesystem::current_path();
    const string executable = escape_path(current_path.string() + "/realcugan-ncnn-vulkan");

    vector<file> input_files;
    cout << "输入待操作的文件名(仅允许png, jpg, webp格式)每行一个. 输入完成后输入E/e结束" << endl;
    while (true) {
        string input;
        getline(cin, input); // 使用 getline 确保读取完整行
        if (input.empty()) continue;
        if (input[0] == 'e' || input[0] == 'E') {
            break;
        }

        // 移除首尾的引号和空格
        if (input.back() == ' ') {
            input.erase(input.back());
        }
        if (input[0] == '\"' || input[0] == '\'') {
            input.erase(0, 1); // 移除第一个字符
        }
        if (input.back() == '\"' || input.back() == '\'') {
            input.erase(input.size() - 1); // 移除最后一个字符
        }

        size_t last_dot = input.find_last_of('.');
        size_t last_slash = input.find_last_of("\\/"); // 支持正斜杠和反斜杠

        // 检查是否找到扩展名
        if (last_dot == string::npos || last_dot == 0 || last_dot == input.size() - 1) {
            cerr << "输入的文件名无效或缺少扩展名: " << input << endl;
            continue;
        }

        // 如果路径没有包含目录
        if (last_slash == string::npos) {
            last_slash = -1; // 这样 last_slash + 1 就为 0
        }

        // 提取文件名和扩展名
        string extension = input.substr(last_dot + 1);

        // 验证扩展名是否有效
        if (extension != "png" && extension != "jpg" && extension != "webp" && extension != "jpeg" && extension != "PNG" && extension != "JPG") {
            cerr << "无效的文件扩展名: " << extension << endl;
            continue;
        }

        file get;
        get.fullfileName = input;
        get.fileWithDir = input.substr(0, last_dot);
        get.extensionName = extension;
        input_files.push_back(get);
    }
    int scale, denoise;
    bool tta;

    if (input_files.size() == 0) {
        return 1;
    }

    cout << "选择超分倍率(2/3/4):";
    while (true) {
        cin >> scale;
        if (scale != 2 && scale != 3 && scale != 4) {
            cout << "无效参数, 请重新输入: ";
            continue;
        }
        break;
    }

    cout << "选择降噪等级(-1/0/1/2/3):";
    while (true) {
        cin >> denoise;
        if (denoise != -1 && denoise != 0 && denoise != 1 && denoise != 2 && denoise != 3) {
            cout << "无效参数, 请重新输入: ";
            continue;
        }
        break;
    }

    cout << "是否启用tta?(y/n)";
    while (true) {
        char in;
        cin >> in;
        in = toupper(in);
        if (in != 'Y' && in != 'N') {
            cout << "无效参数, 请重新输入: ";
            continue;
        }
        if (in == 'Y') {
            tta = true;
        }
        else if (in == 'N') {
            tta = false;
        }
        break;
    }

    cout << endl;
    vector<runtime_error> errors;
    for (auto it = input_files.begin(); it != input_files.end(); it++) {
        try {
            cout << "正在处理: " << it->fullfileName << endl;
            process_image(executable, *it, denoise, scale, tta);
            cout << endl;
        }
        catch (const runtime_error& err) {
            errors.push_back(err);
        }
    }

    cout << (input_files.size() - errors.size()) << " 个图片处理成功, " << errors.size() << " 个文件处理失败." << endl;
    if (errors.size() > 0) {
        cout << endl << "处理失败文件如下: " << endl;
        for (auto it = errors.begin(); it != errors.end(); it++) {
            cout << "   " << it->what() << endl;
        }
    }

    return EXIT_SUCCESS;
}
