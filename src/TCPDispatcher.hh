#ifndef TCPDispatcher_hh
#define TCPDispatcher_hh

#include <string>
#include <Digitizer.hh>
#include <Dispatcher.hh>

typedef struct DigitizerData {
  typedef struct ChannelData {
    uint32_t chID;
    uint32_t offset;
    uint32_t threshold;
    float dynamic_range;
    uint16_t samples[20][500];
    uint16_t patterns[20];
  } ChannelData;

  uint16_t type;
  char name[50];
  uint16_t bits;
  uint16_t samples;
  uint16_t nEvents;
  float ns_sample;
  uint32_t counters[20];
  uint32_t timetags[20];
  uint16_t exttimetags[20];

  ChannelData channels[16];
} DigitizerData;


class TCPDispatcher: public Dispatcher {
public:
  TCPDispatcher() : Dispatcher() {}
  TCPDispatcher(size_t _nEvents,
                std::string _address,
                std::string _port,
                vector<Decoder*> _decoders);
  virtual ~TCPDispatcher();

  string NextPath() { return ""; }
  size_t Digest(vector<Buffer*>& buffers);
  void Dispatch(vector<Buffer*>& buffers);

protected:
  int sockfd;
  std::string address;
  std::string port;

  DigitizerData data;
  std::vector<Decoder*> decoders;
};

#endif

