#ifndef NetDispatcher_hh
#define NetDispatcher_hh

#include <string>
#include <Digitizer.hh>
#include <Dispatcher.hh>

typedef struct {
  uint16_t packet_type;
  uint16_t len;
} NetMeta;

class NetDispatcher: public Dispatcher {
public:
  NetDispatcher() : Dispatcher() {}
  NetDispatcher(size_t _nEvents,
                std::string address,
                int port,
                vector<Decoder*> _decoders);
  virtual ~NetDispatcher();

  string NextPath() { return ""; }
  size_t Digest(vector<Buffer*>& buffers);
  void Dispatch(vector<Buffer*>& buffers);

protected:

  int sockfd;  
  std::string address;
  int port;

  DigitizerData data;
  std::vector<Decoder*> decoders;
};

#endif

