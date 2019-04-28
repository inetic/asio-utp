#include "context_service.hpp"
#include "context.hpp"

using namespace std;
using namespace asio_utp;

asio::io_context::id context_service::id;

void context_service::erase_context(endpoint_type ep)
{
    _contexts.erase(ep);
}

