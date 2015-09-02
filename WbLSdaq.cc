/**
 *  Copyright 2014 by Benjamin Land (a.k.a. BenLand100)
 *
 *  WbLSdaq is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  WbLSdaq is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with WbLSdaq. If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <pthread.h>
#include <iostream>
#include <map>
#include <vector>
#include <stdexcept>
#include <unistd.h>
#include <ctime>
#include <cmath>
#include <csignal>
#include <cstring>

#include "RunDB.hh"
#include "VMEBridge.hh"
#include "V1730_dpppsd.hh"
#include "V1742.hh"

using namespace std;

bool running;

void int_handler(int x) {
    running = false;
}

typedef struct {
    vector<DigitizerSettings*> *settings;
    vector<Digitizer*> *digitizers;
    vector<Buffer*> *buffers;
    pthread_mutex_t *iomutex;
    pthread_cond_t *newdata;
} decode_thread_data;

void *decode_thread(void *_data) {
    signal(SIGINT,int_handler);
    decode_thread_data* data = (decode_thread_data*)_data;
    try {
        while (running) {
            pthread_mutex_lock(data->iomutex);
            for (;;) {
                bool found = !running;
                for (size_t i = 0; i < data->buffers->size(); i++) {
                    found |= (*data->buffers)[i]->fill() > 0;
                }
                if (found) break;
                pthread_cond_wait(data->newdata,data->iomutex);
            }
            for (size_t i = 0; i < data->buffers->size(); i++) {
                size_t sz = (*data->buffers)[i]->fill();
                if (sz > 0) {   
                    (*data->buffers)[i]->dec(sz); // drain
                    cout << (*data->settings)[i]->getIndex() << " " << sz << " bytes." << endl;
                }
            }
            pthread_mutex_unlock(data->iomutex);
        }
    } catch (runtime_error &e) {
        running = false;
        pthread_mutex_lock(data->iomutex);
        cout << "Decode thread aborted: " << e.what() << endl;
        pthread_mutex_unlock(data->iomutex);
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv) {

    if (argc != 2) {
        cout << "./v1730_dpppsd config.json" << endl;
        return -1;
    }

    running = true;
    signal(SIGINT,int_handler);

    cout << "Reading configuration..." << endl;
    
    RunDB db;
    db.addFile(argv[1]);
    RunTable run = db.getTable("RUN");
    
    const int ngrabs = run["events"].cast<int>();
    const string outfile = run["outfile"].cast<string>();
    const int linknum = run["link_num"].cast<int>();
    const int temptime = run["check_temps_every"].cast<int>();
    /*
    int nrepeat;
    if (run.isMember("repeat_times")) {
        nrepeat = run["repeat_times"].cast<int>();
    } else {
        nrepeat = 0;
    }
    */

    cout << "Opening VME link..." << endl;
    
    VMEBridge bridge(linknum,0);
    
    cout << "Setting up digitizers..." << endl;
    
    vector<DigitizerSettings*> settings;
    vector<Digitizer*> digitizers;
    vector<Buffer*> buffers;
    vector<Decoder*> decoders;
    
    vector<RunTable> v1730s = db.getGroup("V1730");
    for (size_t i = 0; i < v1730s.size(); i++) {
        RunTable &tbl = v1730s[i];
        cout << "\tV1730 - " << tbl.getIndex() << endl;
        V1730Settings *stngs = new V1730Settings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1730(bridge,tbl["base_address"].cast<int>()));
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        decoders.push_back(new V1730Decoder((size_t)(ngrabs*1.5),*stngs));
    }
    
    vector<RunTable> v1742s = db.getGroup("V1742");
    for (size_t i = 0; i < v1742s.size(); i++) {
        RunTable &tbl = v1742s[i];
        cout << "\tV1742 - " << tbl.getIndex() << endl;
        V1742Settings *stngs = new V1742Settings(tbl,db);
        settings.push_back(stngs);
        digitizers.push_back(new V1742(bridge,tbl["base_address"].cast<int>()));
        buffers.push_back(new Buffer(tbl["buffer_size"].cast<int>()*1024*1024));
        decoders.push_back(NULL);
    }
    
    cout << "Programming Digitizers..." << endl;
    
    size_t arm_last = 0;
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (!digitizers[i]->program(*settings[i])) return -1;
        if (settings[i]->getIndex() == run["arm_last"].cast<string>()) arm_last = i;
    }
    
    cout << "Starting acquisition..." << endl;
    
    pthread_mutex_t iomutex;
    pthread_cond_t newdata;
    pthread_mutex_init(&iomutex,NULL);
    pthread_cond_init(&newdata, NULL);
    vector<uint32_t> temps;
    
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (i == arm_last) continue;
        digitizers[i]->startAcquisition();
    }
    digitizers[arm_last]->startAcquisition();
    
    decode_thread_data data;
    data.settings = &settings;
    data.digitizers = &digitizers;
    data.buffers = &buffers;
    data.iomutex = &iomutex;
    data.newdata = &newdata;
    pthread_t decode;
    pthread_create(&decode,NULL,&decode_thread,&data);
    
    struct timespec last_temp_time, cur_time;
    clock_gettime(CLOCK_MONOTONIC,&last_temp_time);
    
    //Readout loop
    while (running) {
    
        //Digitizer loop
        for (size_t i = 0; i < digitizers.size() && running; i++) {
            Digitizer *dgtz = digitizers[i];
            if (dgtz->readoutReady()) {
                buffers[i]->inc(dgtz->readoutBLT(buffers[i]->wptr(),buffers[i]->free()));
                pthread_cond_signal(&newdata);
            }
            if (!dgtz->acquisitionRunning()) {
                pthread_mutex_lock(&iomutex);
                cout << "Digitizer " << settings[i]->getIndex() << " aborted acquisition!" << endl;
                pthread_mutex_unlock(&iomutex);
                running = false;
            }
        }
        
        //Temperature check
        clock_gettime(CLOCK_MONOTONIC,&cur_time);
        if (cur_time.tv_sec-last_temp_time.tv_sec > temptime) {
            pthread_mutex_lock(&iomutex);
            last_temp_time = cur_time;
            cout << "Temperature check..." << endl;
            bool overtemp = false;
            for (size_t i = 0; i < digitizers.size() && running; i++) {
                overtemp |= digitizers[i]->checkTemps(temps,60);
                cout << settings[i]->getIndex() << " : [ " << temps[0];
                for (size_t t = 1; t < temps.size(); t++) cout << ", " << temps[t];
                cout << " ]" << endl;
            }
            if (overtemp) {
                cout << "Overtemp! Aborting readout." << endl;
                running  = false;
            }
            pthread_mutex_unlock(&iomutex);
        }
    }
    
    pthread_mutex_lock(&iomutex);
    cout << "Stopping acquisition..." << endl;
    pthread_mutex_unlock(&iomutex);
    
    digitizers[arm_last]->stopAcquisition();
    for (size_t i = 0; i < digitizers.size(); i++) {
        if (i == arm_last) continue;
        digitizers[i]->stopAcquisition();
    }
    
    pthread_cond_signal(&newdata);
    
    // Should add some logic to cleanup memory, but we're done anyway
    
    pthread_exit(NULL);

}
