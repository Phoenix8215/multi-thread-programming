#include "utils.hpp"
#include "logger.hpp"

using namespace std;

// changePath 函数用于根据给定的原始路径 (srcPath)、相对路径 (relativePath)、后缀 (postfix) 和标签 (tag) 生成一个新的路径。
// 假设：
//     srcPath = "/home/user/documents/file.txt"
//     relativePath = "backup/"
//     postfix = ".bak"
//     tag = "v1"
// 示例：
// string newPath = changePath("/home/user/documents/file.txt", "backup/", ".bak", "v1");
// newPath = "/home/user/documents/backup/file-v1.bak"
string changePath(string srcPath, string relativePath, string postfix, string tag){
    int name_l = srcPath.rfind("/");
    int name_r = srcPath.rfind(".");

    int dir_l  = 0;
    int dir_r  = srcPath.rfind("/");

    string newPath;

    newPath = srcPath.substr(dir_l, dir_r + 1);
    newPath += relativePath;
    newPath += srcPath.substr(name_l, name_r - name_l);

    if (!tag.empty())
        newPath += "-" + tag + postfix;
    else
        newPath += postfix;

    return newPath;
}

// loadDataList 函数从给定的文件中读取每一行数据，并将其存储到一个字符串向量中。每一行数据代表一个字符串。
// 假设文件 data.txt 内容如下：
        // line1
        // line2
        // line3
// 调用：
// vector<string> list = loadDataList("data.txt");
// list = {"line1", "line2", "line3"}
vector<string> loadDataList(string file){
    vector<string> list;
    auto *f = fopen(file.c_str(), "r");
    if (!f) LOGE("Failed to open %s", file.c_str());

    char str[512];
    while (fgets(str, 512, f) != NULL) {
        for (int i = 0; str[i] != '\0'; ++i) {
            if (str[i] == '\n'){
                str[i] = '\0';
                break;
            }
        }
        list.push_back(str);
    }
    fclose(f);
    return list;
}
