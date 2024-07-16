#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <cstddef>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include "../Buffer/buffer.h"


class HttpResponse{
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& src_dir, const std::string& path, bool is_keep_alive = false, int code = -1);
    void MakeResponse(Buffer& buff);
    int GetCode() const;
    size_t GetFileLen() const;
    char* GetFile();

private:
    void UnmapFile();
    void ErrorHtml();
    void AddStateLine(Buffer& buff);
    void AddHeader(Buffer& buff);
    void AddContent(Buffer& buff);

    void ErrorContent(Buffer& buff,const std::string& message);
    std::string GetFileType();

private:
    int code_;
    bool is_keep_alive_;
    std::string path_;
    std::string src_dir_;
    char* mm_file_;
    struct stat mm_file_stat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;      // 后缀类型集
    static const std::unordered_map<int, std::string> CODE_STATUS;              // 编码状态集
    static const std::unordered_map<int, std::string> CODE_PATH;                // 编码路径集
};
#endif 