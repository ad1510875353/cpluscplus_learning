/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */
#include "http_request.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HttpRequest::Init()
{
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::parse(Buffer &buff)
{
    const char CRLF[] = "\r\n"; // 行结束符标志(回车换行)
    if (buff.ReadableBytes() <= 0)
    {
        return false;
    }
    while (buff.ReadableBytes() && state_ != FINISH)
    {
        // 从buff中的读指针开始到写指针开始对应区域是未读取的数据
        // 1.如果这块数据中找到以"\r\n"开头的内容，返回"\r\n"开头的位置，这就是对应有效数据的结尾+1
        // 那么[buff.Peek(),lineEnd)是有效数据,清空数据时buff.RetrieveUntil(lineEnd + 2)
        // 2.当找不到的时候，返回的是buff.BeginWrite()，那么[buff.Peek(),lineEnd)仍然是有效数据
        // 只是说buff已经到结尾了，这时候buff.RetrieveUntil(lineEnd)就行了，不然会报错。
        const char *lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd); // 有效数据
        switch (state_)
        {
        case REQUEST_LINE:
            if (!ParseRequestLine_(line)) // 先解析行，转换状态
            {
                return false;
            }
            ParsePath_(); // 定位到资源
            break;
        case HEADERS:
            ParseHeader_(line); // 解析报头，直到遇到空行
            break;
        case BODY:
            ParseBody_(line); // 解析正文段,首先判断是不是post，不然直接结束
            break;
        default:
            break;
        }
        if (lineEnd == buff.BeginWrite())
        {
            // 这种情况是对于post请求，其报文段是没有回车的
            buff.RetrieveUntil(lineEnd);
            break;
        }
        // 把这行数据包括/r/n给取出来
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

void HttpRequest::ParsePath_()
{
    if (path_ == "/")
    {
        path_ = "/index.html";
    }
    else
    {
        for (auto &item : DEFAULT_HTML)
        {
            if (item == path_)
            {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string &line)
{
    LOG_DEBUG("ParseLine : [%s]", line.c_str()); // 打印出来
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten))
    {
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const string &line)
{
    LOG_DEBUG("ParseHeader : [%s]", line.c_str()); // 打印出来
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten))
    {
        header_[subMatch[1]] = subMatch[2];
    }
    else
    {
        // 匹配到空行，进入body字段
        if (method_ == "POST")
        {
            state_ = BODY;
        }
        else
            state_ = FINISH;
    }
}

void HttpRequest::ParseBody_(const string &line)
{
    if (method_ == "POST")
    {
        LOG_DEBUG("ParseBody : [%s], len : %d", line.c_str(), line.size());
        body_ = line;
        ParsePost_();
    }
    state_ = FINISH;
}

int HttpRequest::ConverHex(char ch)
{
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return ch;
}

// 对post字段进行处理
void HttpRequest::ParsePost_()
{
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded")
    {
        // 先对post字段进行解码，获得账号和密码
        ParseFromUrlencoded_();
        if (DEFAULT_HTML_TAG.count(path_))
        {
            // 根据要访问的界面的路径，判断是否是登录或者注册。
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1)
            {
                bool isLogin = (tag == 1);
                // 根据是否登录，分别进行处理，然后重定位到欢迎页。
                if (UserVerify(post_["username"], post_["password"], isLogin))
                {
                    path_ = "/welcome.html";
                }
                else
                {
                    path_ = "/error.html";
                }
            }
        }
    }
}

// 从url中解析键值对，示例：action=user_login&username=%E5%8F%91&password=+%E5%8F%91&rememberme=1
// &是匹配结束，+是空格。上面的是 action=user_login username=%E5%8F%91 password= %E5%8F%91 rememberme=1
void HttpRequest::ParseFromUrlencoded_()
{
    if (body_.size() == 0)
    {
        return;
    }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; i++)
    {
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            // %后面是一个16进制数，转成十进制数的字符串。
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    // 最后没有&，要把它放进去
    if (post_.count(key) == 0 && j < i)
    {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

// 这部分涉及一些sql操作。
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin)
{
    if (name == "" || pwd == "")
    {
        return false;
    }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL *sql;
    // 获取sql连接池的一个连接
    SqlConnRAII sqlRAII(&sql, SqlConnPool::Instance());
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    // 命令
    char order[256] = {0};
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    if (!isLogin)
    {
        flag = true; // 如果不是登录，flag置为true；
    }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order))
    {
        // 查询失败，先清空缓存
        mysql_free_result(res);
        LOG_WARN("qurey failed");
        return false;
    }
    res = mysql_store_result(sql);    // 查询结果
    j = mysql_num_fields(res);        // 列的数量
    fields = mysql_fetch_fields(res); // 查询列的标题

    // 一行一行获取查看结果
    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 登录行为*/
        if (isLogin)
        {
            if (pwd == password)
            {
                flag = true; // 密码正确
            }
            else
            {
                flag = false;
                LOG_DEBUG("pwd error!"); // 密码错误
            }
        }
        else
        {
            flag = false;
            LOG_DEBUG("user used!"); // 注册行为，用户名占用
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag == true)
    {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order))
        {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    // sql已经由RAII对象管理,会自动放回去了
    // SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const
{
    return path_;
}

std::string &HttpRequest::path()
{
    return path_;
}

std::string HttpRequest::method() const
{
    return method_;
}

std::string HttpRequest::version() const
{
    return version_;
}

// 查询header中是否有该字段
bool HttpRequest::IsKeepAlive() const
{
    if (header_.count("Connection") == 1)
    {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}
// 查询post发来的东西，如账户密码等，
std::string HttpRequest::GetPost(const std::string &key) const
{
    assert(key != "");
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}
// 查询post发来的东西，如账户密码等，
std::string HttpRequest::GetPost(const char *key) const
{
    assert(key != nullptr);
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}