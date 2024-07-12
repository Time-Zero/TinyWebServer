# TinyWebServer

# 简介

一个基于C++实现的建议WebServer


# 测试

## Buffer

如果需要测试，请在 `CMakeLists.txt`中将 `BUFFER_TEST`变量设置为TRUE。这样在编译中将会使用Buffer类尝试将 `\test\test.txt`中的小说读取后写入到 `\test\test_out.txt`中

并且由于Buffer类的默认缓冲区大小设置，默认情况下无法完整读写小说，如果想要完整读写请将 `Buffer::ReadFd`中的buffer调大一点
