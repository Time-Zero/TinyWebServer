#include "httpresponse.h"
#include <cassert>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <sys/mman.h>
#include <fcntl.h>
#include "../Log/log.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

HttpResponse::HttpResponse() : 
    code_(-1),
    path_(""),
    src_dir_(""),
    mm_file_(nullptr),
    mm_file_stat_({0})
{
    
}

HttpResponse::~HttpResponse(){
    UnmapFile();
}

int HttpResponse::GetCode() const {
    return code_;
}

char* HttpResponse::GetFile(){
    return mm_file_;
}

size_t HttpResponse::GetFileLen() const {
    return mm_file_stat_.st_size;
}

// 释放文件内存映射
void HttpResponse::UnmapFile(){
    if(mm_file_){
        munmap(mm_file_, mm_file_stat_.st_size);
        mm_file_ = nullptr;
    }
}

void HttpResponse::Init(const std::string& src_dir, const std::string& path, bool is_keep_alive, int code){
    assert(src_dir.size());
    UnmapFile();
    src_dir_ = src_dir;
    path_ = path;
    is_keep_alive_ = is_keep_alive;
    code_ = code;
    mm_file_stat_ = {0};
}

// 判断是不是http错误码响应
void HttpResponse::ErrorHtml(){
    if(CODE_PATH.count(code_) == 1){
        path_ = CODE_PATH.find(code_)->second;
        stat((src_dir_ + path_).c_str(), &mm_file_stat_);
    }
}

void HttpResponse::AddStateLine(Buffer& buff){
    std::string status;
    if(CODE_STATUS.count(code_) == 1){
        status = CODE_STATUS.find(code_)->second;
    }else{
        code_ = 400;
        status = CODE_STATUS.find(code_)->second;
    }
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader(Buffer& buff){
    buff.Append("Connecton: ");
    if(is_keep_alive_){
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }else{
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType() + "\r\n");
}

std::string HttpResponse::GetFileType(){
    std::string::size_type index = path_.find_last_of('.');     // 返回最后一个.的位置
    if(index == std::string::npos){
        return "text/plain";            // 如果找不到，就是"text/plain"类型
    }

    std::string suffix = path_.substr(index);       // 能找到，就通过文件后缀查找文件类型
    if(SUFFIX_TYPE.count(suffix) == 1){
        return SUFFIX_TYPE.find(suffix)->second;
    }

    return "text/plain";                // 如果还是找不到，还是返回这个
}

void HttpResponse::ErrorContent(Buffer& buff,const std::string& message){
    std::string body;
    std::string status;
    
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    
    if(CODE_STATUS.count(code_) == 1){
        status = CODE_STATUS.find(code_)->second;
    }else{
        status = "Bad Request";
    }

    body += std::to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

void HttpResponse::AddContent(Buffer& buff){
    int src_fd = open((src_dir_ + path_).c_str(), O_RDONLY);        // 如果打开资源文件失败
    if(src_fd < 0){
        ErrorContent(buff, "File NotFound");
        return;
    }

    LOG_DEBUG("file path %s ", (src_dir_ + path_).c_str());
    int* mm_ret = (int*)mmap(0, mm_file_stat_.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0);

    if(*mm_ret == -1){      // 如果建立文件内存映射失败
        close(src_fd);
        ErrorContent(buff, "File NotFound");
        return;
    }

    mm_file_ = (char*)mm_ret;
    close(src_fd);
    buff.Append("Content-length: " + std::to_string(mm_file_stat_.st_size) + "\r\n\r\n");
}


// 生成响应报文
void HttpResponse::MakeResponse(Buffer& buff){
    if(stat((src_dir_ + path_).c_str(), &mm_file_stat_) < 0 && S_ISDIR(mm_file_stat_.st_mode)){     // 先看看这个文件存不存在，再看看是不是文件夹
        code_ = 404;
    }else if(!(mm_file_stat_.st_mode & S_IROTH)){           // 如果对访问的资源的权限不足
        code_ = 403;
    }else if(code_ == -1){
        code_ = 200;
    }

    ErrorHtml();                // 如果返回的是错误码，则会在这个函数中打开错误码对应的html
    AddStateLine(buff);         
    AddHeader(buff);
    AddContent(buff);
}
