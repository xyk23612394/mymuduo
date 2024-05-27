#include "EventLoop.h"
#include "Poller.h"
#include "Logger.h"
#include "Channel.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>
#include <errno.h>
//防止一个线程创建多个Eventloop ，thread_local
__thread EventLoop *t_loopInThisThread = nullptr;

//定义默认的Poller io复用的接口超时时间 10s
const int kPollTimeMs = 10000;

//创建wakeupfd，用来唤醒subReactor
int createEventfd()
{
    int evtfd = ::eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n",errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
        :looping_(false)
        ,quit_(false)
        ,callingPendingFunctors_(false)
        ,threadId_(CurrentThread::tid())
        ,poller_(Poller::newDefaultPoller(this))
        ,wakeupFd_(createEventfd())
        ,wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this,threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n",t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }

    //设置wakeupfd的事件类型和发生事件的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个eventloop都将监听wakeupchannel的epollin读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    
    LOG_INFO("EventLoop %p start looping \n",this);
    
    while(!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            //Poller监听哪些channel发生事件，然后上报给eventloop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前Eventloop事件循环需要处理的回调操作
        doPendingFunctor();
    }
    LOG_INFO("EventLoop %p stop looping. \n",this);
    looping_ = false;
}

//退出事件循环 1.loop在自己的线程中调用quit， 2.在非loop的线程中，调用loop的quit
void EventLoop::quit()
{
    quit_ = true;
    if(!isInLoopThread())// 如果在其他线程中，调用的quit   在一个subloop中，调用了mainLoop的quit
    {
        wakeup();
    }
}

//当前loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) // 在当前的loop线程中执行cb
    {
        cb();
    }
    else// 在非当前线程中执行cb，需要唤醒loop所在线程执行cb
    {
        queueInLoop(cb);
    }
}
    //把cb放入队列中，唤醒loop所在线程执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //唤醒相应执行上面回调操作的loop线程
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }

}
    //唤醒loop所在线程的,想wakeupFd_写一个8字节数据，wakeupchannel就发生读事件，
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_,&one,sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n",n);
    }
}
//Eventloop的方法调用-》poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    poller_->hasChannel(channel);
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_,&one,sizeof one);
    if (n != sizeof one)
    {
        LOG_ERROR("Event::handleRead() reads %d bytes instead of 8 \n",n);
    }
}

void EventLoop::doPendingFunctor()//执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor & functor: functors)
    {
        functor(); // 执行当前loop需要执行的回调操作
    }
}