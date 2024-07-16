#include "httprequest.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <regex>
#include <string>
#include <strings.h>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>
#include "../Log/log.h"
#include "mysql.h"
#include "../Pool/sqlconnpool.h"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/index","/register","/login","/welcome","/vedio",
    "/picture"
};

const std::unordered_map<std::string, int> DEFUALT_HTML_TAG{
    {"/register.html",0}, {"/login.html",1}
};


HttpRequest::HttpRequest() :
    state_(REQUEST_LINE),
    method_(""),
    path_(""),
    version_(""),
    body_(""),
    header_(),
    post_()
{

}

void HttpRequest::Init(){
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::Parse(Buffer& buff){
    const char CRLF[] = "\r\n";         // 行结束标志
    if(buff.ReadableBytes() <= 0){
        return false;
    }

    while(buff.ReadableBytes() && state_ != FINISH){
        // 从读空间到写空间找行结束标志
        const char* line_end = std::search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);

        std::string line(buff.Peek(), line_end);

        switch (state_) {
            case REQUEST_LINE:
                if(!ParseRequestLine(line))
                    return false;
                ParsePath();
                break;
            case HEADERS:
                ParseHeader(line);

                if(buff.ReadableBytes() <= 2)       // 如果处理请求体过程中发现剩余待处理内容长度小于2，则说明不是post方式
                    state_ = FINISH;
                break;
            case BODY:
                ParseBody(line);
                break;
            case FINISH:
                break;
            default:
                break;
        }
        if(line_end == buff.BeginWrite())   break;
        buff.RetrieveUntil(line_end + 2);           // 不是很理解这个有什么用
    }

    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

int HttpRequest::ConverHex2Dec(char ch){
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

bool HttpRequest::ParseRequestLine(const std::string& str){
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch sub_match;
    
    if(std::regex_match(str, sub_match, patten)){
        method_ = sub_match[1];
        path_ = sub_match[2];
        version_ = sub_match[3];
        state_ = HEADERS;
        return true;
    }
    
    LOG_ERROR("RequestLine Parse Error!");
    return false;
}

void HttpRequest::ParsePath(){
    if(path_ == "/"){
        path_ = "/index.html";
    }else{
        for(auto &item : DEFAULT_HTML){
            if(item == path_)
                path_ += ".html";
            break;
        }
    }
}

void HttpRequest::ParseHeader(const std::string& str){
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch sub_match;
    if(std::regex_match(str, sub_match, patten)){       // 一直解析请求头，一直到请求头的所有行全部被解析完成
        header_[sub_match[1]]=sub_match[2];
    }else{
        state_ = BODY;
    }
}

void HttpRequest::ParseBody(const std::string& str){
    body_ = str;
    ParsePost();        // 处理请求体，转到处理Post请求
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", str.c_str(), str.size());
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1){
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }

    return false;
}

// 解析URL编码
void HttpRequest::ParseFromUrlencoded(){
    if(body_.size() == 0) return;

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0,  j =0;

    for(; i < n; i++){
        char ch = body_[i];
        switch (ch) {
            case '=':
                key = body_.substr(j, i - j);
                j = i +1;
                break;
            case '+':
                body_[i] = ' ';         // URL编码中，+是空格
                break;
            case '%':
                num = ConverHex2Dec(body_[i + 1]) * 16 + ConverHex2Dec(body_[i + 2]);
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i+=2;
                break;
            case '&':
                value = body_.substr(j, i-1);
                j = i + 1;
                post_[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
    }

    assert(j <= i);
    if(post_.count(key) == 0 && j < i){
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}


void HttpRequest::ParsePost(){
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded"){
        ParseFromUrlencoded();              // 解析URL编码
        if(DEFUALT_HTML_TAG.count(path_)){          //  如果是注册或者登录的访问
            int tag = DEFUALT_HTML_TAG.find(path_)->second;     
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1){
                bool is_login = (tag == 1);         // 如果是登录
                
            }
        }
    }
}

bool HttpRequest::UserVerify(const std::string& name, const std::string&pwd, bool is_login){
    if(name.size() == 0 || pwd.size() == 0) return false;

    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    sql = SqlConnPool::Instance().GetConn();
    if(sql == nullptr)
        return false;

    bool flag = false;
    unsigned int j = 0;
    char order[256];
    bzero(order, 256);
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    if(!is_login)   flag = true;            // 如果是注册用户

    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if(mysql_query(sql, order)){        // 如果sql执行失败
        mysql_free_result(res);
        return false;
    }

    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while(MYSQL_ROW row = mysql_fetch_row(res)){            // 如果有返回值
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);

        if(is_login){           // 如果是登录
            if(pwd == password)             
                flag = true;
            else{
                flag = false;
                LOG_ERROR("%s", "password error!");
            }
        }else{
            flag = false;               // 在注册流程中，如果有返回值，就说明这个用户已经被注册了
            LOG_ERROR("%s", "User Register, but user used");
        }

        mysql_free_result(res);
    }

    // 正常情况下的注册行为，用户名没有被使用
    if(!is_login && flag == true){
        LOG_DEBUG("%s", "User Register");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        
        if(mysql_query(sql, order)){
            LOG_ERROR("%s", "User Register Error, Unknown Error!");
            flag = false;
        }else{
            flag = true;
        }
    }

    SqlConnPool::Instance().FreeConn(sql);
    LOG_DEBUG("%s", "End UserVerify");
    return flag;
}

std::string HttpRequest::path() const {
    return path_;
}

std::string HttpRequest::GetPost(const std::string& key) const{
    assert(key.size());
    if(post_.count(key) == 1){
        return post_.find(key)->second;
    }
    
    return "";
}

std::string HttpRequest::GetPost(const char* key) const{
    assert(key);
    if(post_.count(key) == 1){
        return post_.find(key)->second;
    }

    return "";
}
