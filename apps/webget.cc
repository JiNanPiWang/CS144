#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <format>

using namespace std;

void get_URL( const string& host, const string& path )
{
  // 创建一个 Address 对象，表示要连接的服务器地址和服务名
  auto server_address = Address( host, "http" );

  // 创建一个 TCP Socket 对象，并连接到服务器.
  TCPSocket tcp_socket;
  tcp_socket.connect( server_address );

  // 构建 HTTP 请求
  std::string http_request = format("GET {} HTTP/1.1\r\n", path); // 格式化路径
  http_request += format("Host: {}\r\n", host); // 格式化主机名
  http_request += "Connection: close\r\n"; // 关闭连接
  http_request += "\r\n"; // 空行作为请求头的结束

  // 发送 HTTP 请求
  tcp_socket.write( http_request );

  // 读取服务器响应并存储在 stringstream 中
  stringstream answer;
  while ( true ) // 我感觉套接字的eof不是一个直观的概念，在这里，
  {
    string buffer;
    tcp_socket.read( buffer ); // 从套接字读取数据
    if (tcp_socket.eof()) // 读没了就不读了。详见file_descriptor.cc 100行
      break;
    else
      answer << buffer; // 否则将读取到的数据添加到 stringstream 中
  }

  // 输出服务器响应
  cout << answer.str();
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
