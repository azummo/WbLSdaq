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
    uint16_t samples[20][200];
    uint16_t patterns[20];
  } ChannelData;

  uint16_t type;
  char name[50];
  uint16_t bits;
  uint16_t samples;
  uint16_t nEvents;
  float ns_sample;
  uint16_t channel_enabled_mask;
  uint32_t counters[20];
  uint32_t timetags[20];
  uint16_t exttimetags[20];

  ChannelData channels[16];
} DigitizerData;

typedef struct {
  uint32_t type;
  uint32_t run_number;
  char outfile[200];
  uint32_t run_type;
  uint32_t source_type;
  float source_x;
  float source_y;
  float source_z;
  float source_theta;
  float source_phi;
  int fiber_number;
  float laserball_size;
  float laser_wavelength;
  uint64_t first_event_id;
} RunStart;

typedef struct {
  uint32_t type;
  uint32_t date;
  uint32_t time;
  uint32_t daq_ver;
  uint32_t runmask;
  uint64_t last_event_id;
  uint32_t run_id;
} RunEnd;

/** Labels for received packet types. */
typedef enum {
  CTL_PACKET,
  CMD_PACKET,
  PTB_PACKET,
  DAQ_PACKET,
  RUN_START_PACKET,
  RUN_END_PACKET,
  UNK_PACKET,
} PacketType;

class TCPDispatcher: public Dispatcher {
public:
  TCPDispatcher() : Dispatcher() {}
  TCPDispatcher(size_t _nEvents,
                std::string _address,
                std::string _port,
		RunStart _rs,
                vector<Decoder*> _decoders);
  virtual ~TCPDispatcher();

  string NextPath() { return ""; }
  void End();
  size_t Digest(vector<Buffer*>& buffers);
  void Dispatch(vector<Buffer*>& buffers);
  void Send(void* data, int len, int type);

protected:
  int sockfd;
  std::string address;
  std::string port;
  int runNumber;

  DigitizerData data;
  RunStart rs;
  RunEnd re;
  uint64_t last_timestamp;
  std::vector<Decoder*> decoders;
};

#endif

