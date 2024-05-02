// ejc

#ifndef Dispatcher_hh
#define Dispatcher_hh

#include <iostream>
#include <string>
#include <vector>
#include <Buffer.hh>
using namespace std;

class Dispatcher{
  public:
    Dispatcher();
    Dispatcher(size_t _nEvents, size_t _nStreams);
   ~Dispatcher();

    virtual string NextPath() = 0;
    virtual size_t Digest(vector<Buffer*>& buffers) = 0;
    virtual void Dispatch(vector<Buffer*>& buffers) = 0;
    virtual bool Ready(bool &print, string path);
  protected:
    size_t curCycle;
    size_t nEvents;
    string previous_path;
    vector<size_t> evtsReady;
    struct timespec cur_time, last_time;
};

#endif
