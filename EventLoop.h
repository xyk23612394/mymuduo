#pragma once

#include <functional>
#include <vector>

#include "noncopyable.h"

#include "Timestamp.h"
#include <memory>
#include <mutex>
#include "CurrentThread.h"
#include <atomic>
class Channel;
class Poller;

class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const {return pollReturnTime_;}

    //当前loop中执行cb
    void runInLoop(Functor cb);
    //把cb放入队列中，唤醒loop所在线程执行cb
    void queueInLoop(Functor cb);
    //唤醒loop所在线程的
    void wakeup();
    //Eventloop的方法调用-》poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    bool isInLoopThread()const{return threadId_ == CurrentThread::tid();}
private:

    void handleRead();//wakeup
    void doPendingFunctor();//处理回调

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_; // 原子操作，通过CAS实现
    std::atomic_bool quit_;// 标志退出loop循环
    
    const pid_t threadId_; // 记录当前线程所在的id

    Timestamp pollReturnTime_; // Poller返回发生事件的channel的时间点
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;// 作用，当mainloop获取一个新用户的channel，通过轮训算法选择一个subloop，通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
    // Channel *currentActiveChannel_;
    
    std::atomic_bool callingPendingFunctors_; //标示当前loop是否需要执行的回调操作
    std::vector<Functor> pendingFunctors_;// 存储当前loop需要执行的回调操作
    std::mutex mutex_; //保护vector的线程安全操作
};