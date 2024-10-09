// ejc

#ifndef SocketDispatcher_hh
#define SocketDispatcher_hh

#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <Dispatcher.hh>
using namespace std;

class SocketDispatcher: public Dispatcher{
  public:
    SocketDispatcher();
    SocketDispatcher(size_t _nEvents,
                     string _path,
                     size_t _nStreams);
   ~SocketDispatcher();

    string NextPath();
    size_t Digest(vector<Buffer*>& buffers);
    void Dispatch(vector<Buffer*>& buffers);
    bool Ready();
    void End() {};
  protected:
    string path;
    int sockfd;
    vector<size_t> queued;
};

#endif
