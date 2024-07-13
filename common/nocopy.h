#ifndef NOCOPY_H
#define NOCOPY_H

// 禁止子类复制
class NoCopy{
public:
    ~NoCopy(){};
protected:
    NoCopy(){};
private:
    NoCopy(const NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;
};

#endif