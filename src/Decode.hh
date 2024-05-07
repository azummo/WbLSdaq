// ejc

#ifndef Decode_hh
#define Decode_hh

#include <vector>
#include <H5Cpp.h>
#include <Buffer.hh>
#include <Digitizer.hh>
#include <RunType.hh>

using namespace std;
using namespace H5;

extern bool stop;
bool decode_running;

void int_handler(int x) {
    if (stop) exit(1);
    stop = true;
}

typedef struct {
    vector<Buffer*> *buffers;
    vector<Decoder*> *decoders;
    pthread_mutex_t *iomutex;
    pthread_cond_t *newdata;
    string config;
    RunType *runtype;
} decode_thread_data;

void *decode_thread(void *_data) {
    signal(SIGINT,int_handler);
    decode_thread_data* data = (decode_thread_data*)_data;
    
    vector<size_t> evtsReady(data->buffers->size());
    data->runtype->begin();
    try {
        decode_running = true;
        while (decode_running) {
            pthread_mutex_lock(data->iomutex);
            for (;;) {
                bool found = stop;
                for (size_t i = 0; i < data->buffers->size(); i++) {
                    found |= (*data->buffers)[i]->fill() > 0;
                }
                if (found) break;
                pthread_cond_wait(data->newdata,data->iomutex);
            }
            
            size_t total = 0;
            for (size_t i = 0; i < data->decoders->size(); i++) {
                size_t sz = (*data->buffers)[i]->fill();
                if (sz > 0) (*data->decoders)[i]->decode(*(*data->buffers)[i]);
                size_t ev = (*data->decoders)[i]->eventsReady();
                evtsReady[i] = ev;
                total += ev;
            }
            
            if (stop && total == 0) {
                decode_running = false;
            } else if (stop || data->runtype->writeout(evtsReady)) {
                Exception::dontPrint();
                
                string fname = data->runtype->fname() + ".h5"; 
                
                H5File file(fname, H5F_ACC_TRUNC);
                data->runtype->write(file);
                  
                DataSpace scalar(0,NULL);
                Group root = file.openGroup("/");
               
//              StrType configdtype(PredType::C_S1, data->config.size());
//              Attribute config = root.createAttribute("run_config",configdtype,scalar);
//              config.write(configdtype,data->config.c_str());
                
                int epochtime = time(NULL);
                Attribute timestamp = root.createAttribute("created_unix_timestamp",PredType::NATIVE_INT,scalar);
                timestamp.write(PredType::NATIVE_INT,&epochtime);
                
                for (size_t i = 0; i < data->decoders->size(); i++) {
                    (*data->decoders)[i]->writeOut(file,evtsReady[i]);
                }
                
                decode_running = data->runtype->keepgoing();
            }
            pthread_mutex_unlock(data->iomutex);
        }
        stop = true;
    } catch (runtime_error &e) {
        pthread_mutex_unlock(data->iomutex);
        stop = true;
        pthread_mutex_lock(data->iomutex);
        cout << "Decode thread aborted: " << e.what() << endl;
        pthread_mutex_unlock(data->iomutex);
    }
    pthread_exit(NULL);
}

#endif
