#include "Buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

//从fd上读取数据 Poller工作在LT模式上
//Buffer缓冲区有大小，但是从fd上读数据却不知道tcp数据最终的大小
size_t Buffer::readFd(int fd ,int * saveErrno)
{
    char extrabuf[65536] = {0}; //栈上的内存空间 64k

    struct iovec vec[2];
    const size_t writable = writableBytes(); //buffer底层缓冲区剩余可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;    

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd,vec,iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) //buffer够存数据
    {
        writerIndex_ += n;
    }
    else //extrabuf 也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf,n - writable);//writerIndex_开始写，n-writable大小的数据
    }
    return n;
}

//通过fd发送数据
size_t Buffer::writeFd(int fd,int * saveError)
{
    ssize_t n = ::write(fd,peek(),readableBytes());
    if (n < 0)
    {
        *saveError = errno;
    }
    return n;
}