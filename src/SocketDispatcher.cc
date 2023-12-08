// ejc

#include <SocketDispatcher.hh>

SocketDispatcher::SocketDispatcher(): Dispatcher(){
    /* */
}

SocketDispatcher::SocketDispatcher(size_t _nEvents,
                                   string _path,
                                   size_t _nStreams):
                                   Dispatcher(_nEvents,
                                              _nStreams){
    this->path = _path;
    this->queued = vector<size_t>(_nStreams, 0);

    int domain = AF_INET;
    int portno = 8765;

    this->sockfd = socket(domain, SOCK_STREAM, 0);
    int s;

    switch (domain) {
      case AF_UNIX: {
        struct sockaddr_un saddr;
        saddr.sun_family = AF_UNIX;
        sprintf(saddr.sun_path, "%s", this->path.c_str());
        s = connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr));
	break;
      }
      case AF_INET: {
        cerr << "eh?" << endl;
        struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = INADDR_ANY;
        saddr.sin_port = htons(portno);
        s = connect(sockfd, (struct sockaddr*) &saddr, sizeof(saddr));
	break;
      }
      default:
        pthread_exit(NULL);
    }

    if (s < 0) {
        cerr << "error during socket connection:" << endl;
        if (errno == EACCES){
            cerr << "\t"
                 << "EACCES: "
                 << "For UNIX domain sockets, which are identified by pathname: Write permission is denied on the socket file, or search permission is denied for one of the directories in the path prefix."
                 << endl;
        }
        if (errno == EADDRINUSE){
            cerr << "\t"
                 << "EADDRINUSE: "
                 << "Local address is already in use."
                 << endl;
        }
        if (errno == EAGAIN){
            cerr << "\t"
                 << "EAGAIN: "
                 << "For nonblocking UNIX domain sockets, the socket is nonblocking, and the connection cannot be completed immediately. For other socket families, there are insufficient entries in  the  routing cache."
                 << endl;
        }
        if (errno == EALREADY){
            cerr << "\t"
                 << "EALREADY: "
                 << "The  socket is nonblocking and a previous connection attempt has not yet been completed."
                 << endl;
        }
        if (errno == EBADF){
            cerr << "\t"
                 << "EBADF: "
                 << "sockfd is not a valid open file descriptor."
                 << endl;
        }
        if (errno == ECONNREFUSED){
            cerr << "\t"
                 << "ECONNREFUSED: "
                 << "A connect() on a stream socket found no one listening on the remote address."
                 << endl;
        }
        if (errno == EFAULT){
            cerr << "\t"
                 << "EFAULT: "
                 << "The socket structure address is outside the user's address space."
                 << endl;
        }
        if (errno == EINPROGRESS){
            cerr << "\t"
                 << "EINPROGRESS: "
                 << "The socket is nonblocking and the connection cannot be completed immediately."
                 << endl;
        }
        if (errno == EINTR){
            cerr << "\t"
                 << "EINTR: "
                 << "The system call was interrupted by a signal."
                 << endl;
        }
        if (errno == EISCONN){
            cerr << "\t"
                 << "EISCONN: "
                 << "The socket is already connected."
                 << endl;
        }
        if (errno == ENOTSOCK){
            cerr << "\t"
                 << "ENOTSOCK: "
                 << "The file descriptor sockfd does not refer to a socket."
                 << endl;
        }
        if (errno == EPROTOTYPE){
            cerr << "\t"
                 << "EPROTOTYPE: "
                 << "The  socket  type  does not support the requested communications protocol."
                 << endl;
        }
        if (errno == ETIMEDOUT){
            cerr << "\t"
                 << "ETIMEDOUT: "
                 << "Timeout while attempting connection."
                 << endl;
        }
        pthread_exit(NULL);
    }
}

SocketDispatcher::~SocketDispatcher(){
    /* */
}

string SocketDispatcher::NextPath(){
    string rv = this->path;
    return rv;
}

size_t SocketDispatcher::Digest(vector<Buffer*>& buffers){
    // TODO assert queued.size() == buffers.size()
    size_t total = 0;
    for (size_t i = 0; i < buffers.size(); i++) {
        size_t sz = buffers[i]->fill();
        this->queued[i] = sz;
        total += sz;
    }
    return total;
}

// TODO
void SocketDispatcher::Dispatch(vector<Buffer*>& buffers){
    // TODO assert queued.size() == buffers.size()
    string path = this->NextPath();
    cout << "Flushing data to " << path << endl;
    for (size_t i = 0 ; i < buffers.size() ; i++){
        size_t writeable = buffers[i]->fill();
        size_t written = send(sockfd,
                              buffers[i]->rptr(),
                              writeable,
                              0);
        if (written != writeable){
            cerr << "warning: only "
                 << to_string(written)
                 << " of "
                 << to_string(writeable)
                 << " bytes written to socket"
                 << endl;
        }
        buffers[i]->dec(written);
        this->queued[i] -= written;
        if (0 < this->queued[i]){
            cerr << "warning: non-zero byes queued after writing to socket"
                 << endl;
        }
    }

    this->curCycle++;
}

bool SocketDispatcher::Ready(){
    bool rv = true;
    for (size_t i = 0 ; i < this->queued.size() ; i++){
        if (this->queued[i] == 0){
            rv = false;
        }
    }
    return rv;
}
