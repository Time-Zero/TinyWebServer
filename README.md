# TinyWebServer

# 简介

一个基于C++实现的简单WebServer，可以正常运行，只需要 `apt`安装一下 `mysql`的连接库即可

# 测试

## Buffer

如果需要测试，请在 `CMakeLists.txt`中将 `BUFFER_TEST`变量设置为TRUE。这样在编译中将会使用Buffer类尝试将 `\test\test.txt`中的小说读取后写入到 `\test\test_out.txt`中

并且由于Buffer类的默认缓冲区大小设置，默认情况下无法完整读写小说，如果想要完整读写请将 `Buffer::ReadFd`中的buffer调大一点

## BlockQueue

同上，只需要将对应的变量设置为 `true`

## Log

同上

## ThreadPool

同上


# 优化点

1. 抛弃STL库正则，尝试使用Boost正则，STL正则性能实在是烂
