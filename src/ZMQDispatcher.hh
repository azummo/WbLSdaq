#ifndef ZMQDispatcher_hh
#define ZMQDispatcher_hh

#include <string>
#include <Digitizer.hh>
#include <Dispatcher.hh>
#include <zmq.hpp>

class ZMQDispatcher: public Dispatcher {
public:
  ZMQDispatcher() : Dispatcher() {}
  ZMQDispatcher(size_t _nEvents,
                std::string address,
                vector<Decoder*> _decoders);
 ~ZMQDispatcher();

  string NextPath() { return ""; }
  size_t Digest(vector<Buffer*>& buffers);
  void Dispatch(vector<Buffer*>& buffers);

protected:
  std::string address;
  zmq::context_t* context;
  zmq::socket_t* socket;

  DigitizerData data;
  std::vector<Decoder*> decoders;
};

#endif

