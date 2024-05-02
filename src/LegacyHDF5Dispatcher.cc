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
/*
        this->decoders[i]->pack(&data, this->evtsReady[i]);

        V1730Settings& settings = *((V1730Settings*)decoders[i]->getSettings());  // :(

        Group cardgroup = file.createGroup("/"+settings.getIndex());

        DataSpace scalar(0,NULL);

        Attribute bits = cardgroup.createAttribute("bits",PredType::NATIVE_UINT32,scalar);
        bits.write(PredType::NATIVE_INT32, &data.bits);

        Attribute ns_sample = cardgroup.createAttribute("ns_sample",PredType::NATIVE_DOUBLE,scalar);
        ns_sample.write(PredType::NATIVE_DOUBLE, &data.ns_sample);

        Attribute samples = cardgroup.createAttribute("samples",PredType::NATIVE_UINT32,scalar);
        samples.write(PredType::NATIVE_UINT32, &data.samples);

        hsize_t dims[1];
        dims[0] = nEvents;
        DataSpace space(1, dims);
        DataSet counters_ds = cardgroup.createDataSet("counters", PredType::NATIVE_UINT32, space);
        counters_ds.write(&data.counters, PredType::NATIVE_UINT32);
        DataSet timetags_ds = cardgroup.createDataSet("timetags", PredType::NATIVE_UINT32, space);
        timetags_ds.write(&data.timetags, PredType::NATIVE_UINT32);

        for (size_t i = 0; i < 16; i++) {
            if (!settings.getEnabled(i)) continue;

            DigitizerData::ChannelData& ch = data.channels[i];

            string chname = "ch" + to_string(ch.chID);
            Group group = cardgroup.createGroup(chname);
            string groupname = "/"+settings.getIndex()+"/"+chname;

            Attribute offset = group.createAttribute("offset",PredType::NATIVE_UINT32,scalar);
            offset.write(PredType::NATIVE_UINT32, &ch.offset);

            Attribute threshold = group.createAttribute("threshold",PredType::NATIVE_UINT32,scalar);
            threshold.write(PredType::NATIVE_UINT32, &ch.threshold);

            Attribute dynamic_range = group.createAttribute("dynamic_range",PredType::NATIVE_DOUBLE,scalar);
            dynamic_range.write(PredType::NATIVE_DOUBLE, &ch.dynamic_range);

            hsize_t dimensions[2];
            dimensions[0] = data.nEvents;
            dimensions[1] = settings.getRecordLength();

            DataSpace samplespace(2, dimensions);
            DataSpace metaspace(1, dimensions);

            DataSet samples_ds = file.createDataSet(groupname+"/samples", PredType::NATIVE_UINT16, samplespace);
            samples_ds.write(&ch.samples[i], PredType::NATIVE_UINT16);

            DataSet patterns_ds = file.createDataSet(groupname+"/patterns", PredType::NATIVE_UINT16, metaspace);
            patterns_ds.write(&ch.patterns[i], PredType::NATIVE_UINT16);
        }
    */
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
