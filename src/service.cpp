#include "service.hpp"
#include "context.hpp"

using namespace std;
using namespace asio_utp;

asio::io_context::id service::id;

void service::erase_context(endpoint_type ep)
{
    _contexts.erase(ep);
}

