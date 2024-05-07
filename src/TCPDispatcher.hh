#ifndef TCPDispatcher_hh
#define TCPDispatcher_hh

#include <string>
#include <Digitizer.hh>
#include <Dispatcher.hh>

class TCPDispatcher: public Dispatcher {
public:
  TCPDispatcher() : Dispatcher() {}
  TCPDispatcher(size_t _nEvents,
                std::string _address,
                std::string _port,
                vector<Decoder*> _decoders);
 ~TCPDispatcher();

  string NextPath() { return ""; }
  size_t Digest(vector<Buffer*>& buffers);
  void Dispatch(vector<Buffer*>& buffers);

protected:
  int sockfd;

  DigitizerData data;
  std::vector<Decoder*> decoders;
};

#endif

