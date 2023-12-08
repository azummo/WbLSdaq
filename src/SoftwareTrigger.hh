// ejc

#ifndef SoftwareTrigger_hh
#define SoftwareTrigger_hh

#include <vector>
#include <Buffer.hh>
#include <Digitizer.hh>
#include <RunType.hh>

#include <sys/types.h>

using namespace std;
using namespace H5;

extern bool stop;
bool software_trigger_running;

typedef struct {
    vector<DigitizerSettings*> *settings;
    vector<Digitizer*> *digitizers;
    RunTable run;
} software_trigger_thread_data;

void *software_trigger_thread(void *_data) {
    software_trigger_thread_data* data = (software_trigger_thread_data*)_data;
    vector<DigitizerSettings*>& settings = *(data->settings);
    vector<Digitizer*>& digitizers = *(data->digitizers);
    RunTable run = data->run;
    while (!stop) {
        for (size_t i = 0; i < digitizers.size(); i++) {
            digitizers[i]->startAcquisition();
            for (size_t j = 0 ; j < run["soft_trig"].getArraySize(); j++){
                if (run.isMember("soft_trig") && settings[i]->getIndex() == run["soft_trig"][j].cast<string>()) {
//                  cout << "Software triggering " << settings[i]->getIndex() << endl;
                    digitizers[i]->softTrig();
                }
            }
        }
        usleep(100);
    }
    pthread_exit(NULL);
}

#endif
