# Check0

我们需要写一个程序，得到一个网页的具体信息。需要用到 $util/socket.hh$ and $util/address.hh$。刚开始看可能有点懵，这两个文件都很大，如果不知道套接字socket是什么的话，那就更不知道该怎么办了。这里简单的介绍一下套接字，如果了解过的话， 可以不用看。

### 套接字是什么

套接字可以被简单的认为是两部计算机之间通信的“电话”，在生活中，我们可以通过电话这个东西，给我们的小伙伴打电话，聊聊天，套接字相当于计算机之间的电话，一个程序可以在其 Socket 上输入另一个程序的网络地址和端口号，然后可以通过这个连接来发送数据到程序 B，或者从程序 B 接收数据。

### 如何使用套接字

套接字也有一些参数，比如程序之间需要用什么样的协议来“打电话”，像我们日常打电话一样，两个人都是用英语，还是都用中文？我说错了一个字或者说漏了一句话，要不要重新告诉你？程序之间打电话，也需要遵循一些协议，举个例子

```c++
Socket(AF_INET, SOCK_STREAM, 0); // domain, type, protocol
```

- `AF_INET` 是地址族（Address Family）的一种，表示我们采用的是 IPV4 网络协议，即我们沟通使用的电话线路是 IPV4 电话线路。
- `SOCK_STREAM` 是 Socket 类型的一种，代表面向连接的，可靠的，使用 TCP 协议进行数据传输，即电话是可以靠的，我们会确保电话线路的连接是通畅的。
- 对于大多数情况，我们可以将 `protocol` 简单地设置为 0，让系统自动为我们选择合适的协议。

这一个步骤相当于创造了一个电话，用于程序来打。

##  3.4 Writing webget

我们需要完成$webget.cc$里面的$get_URL$函数，得到一个网页的具体信息，也就是给对方打电话，然后让对方告诉我们它的信息。需要用到 $util/socket.hh$ and $util/address.hh$。

首先我们要知道，我们“打电话”的对象是谁，也就是需要知道套接字的目标地址。函数参数里面的$host$，就是我们的目标地址，比如题目的要求是：$"cs144.keithw.org"$。通过观察套接字和题目提示，我们发现要使用$util/address.hh$里面的$Address$类来确定地址。注意$Address$类的构造函数：

```c++
  //! Construct by resolving a hostname and servicename.
  Address( const std::string& hostname, const std::string& service );

  //! Construct from dotted-quad string ("18.243.0.1") and numeric port.
  explicit Address( const std::string& ip, std::uint16_t port = 0 );
```

一个是通过$hostname$和$servicename$来构造，这里的$hostname$也就是我们的网址，$servicename$就是我们访问的服务名，这里就是$http$。下面那个构造函数即是通过$ip$和端口号来构造地址对象，这里注意别用错了。

下面就可以构造我们的“电话”了，构造对象的方法可以参考上面的如何使用套接字，$util/socket.hh$里面有很多种套接字，按照题目的要求，我们使用$TCPSocket$类。然后根据$Socket$的方法，构造套接字对象，并连接$host$，也就是开始打电话给对方。

```c++
  // 创建一个 TCP Socket 对象，并连接到服务器.
  TCPSocket tcp_socket;
  tcp_socket.connect( server_address );
```

打电话的内容是什么呢，就是题目要求的http请求了，参考pdf最上面的内容。

```c++
  // 构建 HTTP 请求
  std::string http_request = format("GET {} HTTP/1.1\r\n", path); // 格式化路径
  http_request += format("Host: {}\r\n", host); // 格式化主机名
  http_request += "Connection: close\r\n"; // 关闭连接
  http_request += "\r\n"; // 空行作为请求头的结束

  // 发送 HTTP 请求
  tcp_socket.write( http_request );
```

我们说完之后，就是听对方说了什么，对方可能不会一句话讲完，我们需要耐心的不断听它说了什么，直到它说完，也就是$tcp_socket.eof()$等于$true$的时候，我们就结束这次的电话。

```c++
  // 读取服务器响应并存储在 stringstream 中
  stringstream answer;
  while ( true )
  {
    string buffer;
    tcp_socket.read( buffer ); // 从套接字读取数据
    if (tcp_socket.eof()) // 读没了就不读了。详见file_descriptor.cc 100行
      break;
    else
      answer << buffer; // 否则将读取到的数据添加到 stringstream 中
  }
```

再输出answer，就结束了！可以测一下自己的结果了

```bash
$ cmake --build build --target check_webget
Test project /home/acs/works/CS144/build
    Start 1: compile with bug-checkers
1/2 Test #1: compile with bug-checkers ........   Passed    2.57 sec
    Start 2: t_webget
2/2 Test #2: t_webget .........................   Passed    1.04 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =   3.60 sec
Built target check_webget
```



