// ejc

#include <LegacyHDF5Dispatcher.hh>
#include <V1730.hh>

using namespace H5;

LegacyHDF5Dispatcher::LegacyHDF5Dispatcher(): Dispatcher() {}

LegacyHDF5Dispatcher::LegacyHDF5Dispatcher(size_t _nEvents,
                                           string _basename,
                                           vector<Decoder*> _decoders)
    : Dispatcher(_nEvents, _decoders.size()),
      basename(_basename),
      decoders(_decoders) {}


LegacyHDF5Dispatcher::~LegacyHDF5Dispatcher() {}


string LegacyHDF5Dispatcher::NextPath(){
  return this->basename + "." + to_string(this->curCycle) + ".h5";
}


size_t LegacyHDF5Dispatcher::Digest(vector<Buffer*>& buffers){
    // TODO assert decoders->size() == buffers.size()
    size_t total = 0;
    for (size_t i = 0; i < this->decoders.size(); i++) {
        size_t sz = buffers[i]->fill();
        if (sz > 0) (this->decoders)[i]->decode(*buffers[i]);
        size_t ev = (this->decoders)[i]->eventsReady();
        this->evtsReady[i] = ev;
        total += ev;
    }
    return total;
}

void LegacyHDF5Dispatcher::Dispatch(vector<Buffer*>& buffers){
    string fname = this->NextPath();
    // cout << "Saving data to " << fname << endl;
    
    H5File file(fname, H5F_ACC_TRUNC);
    this->Initialize(file);
      
    DataSpace scalar(0, NULL);
    Group root = file.openGroup("/");
   
    //  TODO can we finagle the config info to here? :)
    //  StrType configdtype(PredType::C_S1, data->config.size());
    //  Attribute config = root.createAttribute("run_config",configdtype,scalar);
    //  config.write(configdtype,data->config.c_str());
    
    int epochtime = time(NULL);
    Attribute timestamp = root.createAttribute("created_unix_timestamp",
                                               PredType::NATIVE_INT,scalar);
    timestamp.write(PredType::NATIVE_INT, &epochtime);

    for (size_t i = 0; i < this->decoders.size(); i++) {
        decoders[i]->writeOut(file, evtsReady[i]);
    }

    this->curCycle++;
}


void LegacyHDF5Dispatcher::Initialize(H5File &file){
    double time_int = (cur_time.tv_sec - last_time.tv_sec)+1e-9*(cur_time.tv_nsec - last_time.tv_nsec);
    
    DataSpace scalar(0,NULL);
    Group root = file.openGroup("/");
    
    Attribute runtime = root.createAttribute("file_runtime",PredType::NATIVE_DOUBLE,scalar);
    runtime.write(PredType::NATIVE_DOUBLE,&time_int);
    
    uint32_t timestamp = time(NULL);
    Attribute tstampattr = root.createAttribute("creation_time",PredType::NATIVE_UINT32,scalar);
    tstampattr.write(PredType::NATIVE_UINT32,&timestamp);

    last_time = cur_time;
}
