#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return this->has_closed;
}

void Writer::push( string data )
{
  if (data.empty())
    return;
  if ( this->has_closed )
    throw std::runtime_error( "Writer has already been closed" );
  if ( data.size() > this->available_capacity() )
  {
    this->cumulatively_bytes_writen += this->available_capacity();
    this->str += data.substr( 0, this->available_capacity() );
  }
  else
  {
    this->cumulatively_bytes_writen += data.size();
    this->str += data;
  }
}

void Writer::close()
{
  has_closed = true;
}

uint64_t Writer::available_capacity() const
{
  return this->capacity_ - this->reader().bytes_buffered();
}

uint64_t Writer::bytes_pushed() const
{
  return this->cumulatively_bytes_writen;
}

bool Reader::is_finished() const
{
  if (this->writer().is_closed() && this->bytes_buffered() == 0)
    return true;
  return false;
}

uint64_t Reader::bytes_popped() const
{
  return cumulatively_bytes_popped;
}

string_view Reader::peek() const
{
  return {this->str};
}

void Reader::pop( uint64_t len )
{
  if ( len > bytes_buffered() )
    throw std::runtime_error( "Not enough data to pop" );
  this->str = this->str.substr( len );
  cumulatively_bytes_popped += len;
}

uint64_t Reader::bytes_buffered() const
{
  return this->str.size();
}
