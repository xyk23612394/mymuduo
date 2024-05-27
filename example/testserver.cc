#include <mymuduo/TcpServer.h>
#include <string>
#include <mymuduo/Logger.h>
#include <functional>

class EchoServer
{
public:
    EchoServer(EventLoop *loop,const InetAddress &addr,const std::string& name)
        :server_(loop,addr,name)
        ,loop_(loop)
        {
            //注册回调函数
            server_.setConnectionCallback(std::bind(&EchoServer::onConnection,this,std::placeholders::_1));
            server_.setMessageCallback(std::bind(&EchoServer::onMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
            //设置合适的loop线程数量
            server_.setThreadNum(3);
        }
    
    void start()
    {
        server_.start();
    }
private:

    //连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            LOG_INFO("Connection up : %s",conn->peerAddress().toIpPort().c_str());            
        }
        else
        {
            LOG_INFO("Connection Down : %s",conn->peerAddress().toIpPort().c_str());
        }
    }

    //可读写事件回调
    void onMessage(const TcpConnectionPtr &conn,Buffer *buf,Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();//关闭写端 EPOLLHUP -》closeCallback_
    }
    EventLoop *loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop,addr,"EchoServer-01"); // Acceptor non-blocking  listenfd create bind
    server.start(); //listen loopthread listenfd -> acceptChannel ->mainloop ->
    loop.loop(); //启动mainloop底层poller
    
    return 0; 
}