// Ed Callaghan
// Licensing statement goes here

#include <CONETNetwork.hh>
using namespace std;

int CONETNetwork::max_handles = 8;
const std::string CONETNetwork::error_codes[15] = {"Operation completed successfully", "VME bus error during the cycle", "Communication error", "Unspecified error", "Invalid parameter", "Invalid Link Type", "Invalid device handler", "Communication Timeout", "Unable to Open the requested Device", "Maximum number of devices exceeded", "The device is already opened", "Not supported function", "There aren't boards controlled by that Bridge", "Communication terminated by the Device", "The Base Address is not supported"};

CONETNetwork::CONETNetwork(int link) {
    this->link = link;
    this->handles = vector<int>(CONETNetwork::max_handles, 0);
    this->valids = vector<bool>(CONETNetwork::max_handles, false);

    // try to open every node in a maximal daisy chain
    // could imagine specifying the size in the network configuration...
    // that's not a bad idea
    int handle;
    for (int i = 0 ; i < CONETNetwork::max_handles ; i++){
        CAENComm_ErrorCode res = CAENComm_OpenDevice(CAENComm_OpticalLink,
                                                     this->link,
                                                     i,
                                                     0,
                                                     &handle);
        if (res) {
            stringstream err;
            err << error_codes[-res] << " :: Could not open CONET node" << i << "!";
            throw runtime_error(err.str());
        }
        else { // res == 0 indicates success
            this->handles[i] = handle;
            this->valids[i] = true;
        }
    }
}

CONETNetwork::~CONETNetwork() noexcept(false) {
    // close all nodes which were marked as valid
    int handle;
    for (int i = 0 ; i < CONETNetwork::max_handles ; i++){
        if (this->valids[i]){
            handle = this->handles[i];
            CAENComm_ErrorCode res = CAENComm_CloseDevice(handle);
            if (res) {
                stringstream err;
                err << error_codes[-res] << " :: Could not close CONET node" << i << "!";
                throw runtime_error(err.str());
            }
            else { // res == 0 indicates success
                this->handles[i] = 0;
                this->valids[i] = false;
            }
        }
    }
}
