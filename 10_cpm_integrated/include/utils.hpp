#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <string>
#include <vector>

// 比较两个数的最小值
#define min(x, y) ((x) < (y) ? (x) : (y))
std::string changePath(std::string srcPath, std::string relativePath, std::string postfix, std::string tag);
std::vector<std::string> loadDataList(std::string file);

#endif __UTILS_HPP__
