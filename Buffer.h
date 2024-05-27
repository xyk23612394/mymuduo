#pragma once

#include <vector>
#include <string>
#include <algorithm>
//底层缓冲区类型
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
            :buffer_(kCheapPrepend + initialSize)
            ,readerIndex_(kCheapPrepend)
            ,writerIndex_(kCheapPrepend)
            {}
    
    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes()const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    //返回缓冲区可读数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    //onMessage string <- Buffer
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
            readerIndex_ += len; //应用只读取了可读缓冲区数据的一部分，就是len，还剩下readerIndex_ += len
        }
        else
        {
            //len == readableBytes()
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_  = kCheapPrepend;
    }

    //把onmessage函数上报的buffer数据转换成string数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(),len);
        retrieve(len);//上面一句把缓冲区中的可读数据已经读取出来，这里需要对缓冲区进行复位操作
        return result;
    }

    //buffer_.size() - writerIndex_  len
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);//扩容函数
        }
        
        
    }

    //把[data ,data + len]内存上的数据添加到writable缓冲区当中
    void append(const char *data,size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data,data + len, beginWrite());
        writerIndex_ += len;
    }

    char * beginWrite()
    {
        return begin() + writerIndex_;
    }
    const char * beginWrite()const
    {
        return begin() + writerIndex_;
    }

    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_+len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,begin() + writerIndex_,begin() + kCheapPrepend);
            readerIndex_  = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    
    //通过fd读取数据
    size_t readFd(int fd ,int * saveError);

    //通过fd发送数据
    size_t writeFd(int fd,int * saveError);
private:
    char * begin()
    {
        return &*buffer_.begin();//vector底层数组首元素的地址，也是数组的起始地址
    }


    const char* begin() const
    {
        return &*buffer_.begin();
    }
    std::vector<char> buffer_; 
    size_t readerIndex_;
    size_t writerIndex_;

};