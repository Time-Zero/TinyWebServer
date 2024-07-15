#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "../Buffer/buffer.h"

class HttpRequest{
public:
    enum PARSE_STATE{
        REQUEST_LINE=0,
        HEADERS=1,
        BODY=2,
        FINISH=3
    };

    HttpRequest();
    ~HttpRequest();

    bool Parse(Buffer& buff);
    bool IsKeepAlive() const;

    std::string path() const;
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

private:
    static int ConverHex2Dec(char ch);

    bool ParseRequestLine(const std::string& str);
    void ParseHeader(const std::string& str);
    void ParseBody(const std::string& str);

    void ParsePath();
    void ParsePost();
    void ParseFromUrlencoded();
    bool UserVerify(const std::string& name, const std::string&pwd, bool is_login);


private:
    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;


    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;

};
#endif