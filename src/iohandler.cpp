#include "iohandler.h"
#include "fileops.h"
#include <iostream>
/*
* i have only a couple of hours as i need to travel.
* so quickly wrapping. This is more of a concept
* implementation with the goal of keeping workers
* always busy.
* */

namespace fileio {

IoHandler& IoHandler::instance() {
    static IoHandler ioh;
    return ioh;

}

void	IoHandler::start(size_t numReaders, size_t numWriters) {
    if (started_) {
        throw std::runtime_error("IoHandler already started");
    }
    auto numThrds= numReaders + numWriters;
    if (numThrds == 0) {
        throw std::invalid_argument{"sync mode not supported"};
    }
    if ((numReaders == 0) || (numWriters == 0)) {
        throw std::invalid_argument{"both rdrs&writers need to be configured"};
    }
    numReaders_ = numReaders;
    numWriters_	= numWriters;
    ioCtxts_.resize(numThrds);
    auto status = std::make_shared< StatusCommand>();
    for (auto i = 0; i < numThrds; ++i) {
        ioCtxts_[i]=std::make_unique<IoCtxt>(i);
        ioCtxts_[i]->thrd_ =std::move(std::thread(&IoHandler::iothrd, this, i));
        post(i,status);
        if (!status->waitResult(10)) {
            throw std::runtime_error("cannot start i/o thread");
        }
        status->reset();

    }
    started_=true;
    std::cout << "iohandler started all ioworkers:" << numThrds << std::endl;
}

IoHandler::~IoHandler() {
	stop();
}

void IoHandler::stop() {
    if (!started_)
        return;
    auto numThrds{numReaders_ + numWriters_};
    auto stop{std::make_shared< StopCommand>()};
    for (auto i = 0; i < numThrds; ++i) {
        post(i, stop);
        if (!stop->waitResult(10)) {
            throw std::runtime_error("cannot stop i/o thread");
        }
        stop->reset();
        ioCtxts_[i]->thrd_.join();
    }
    started_ = false;
    std::cout << "iohandler stopped all ioworkers:" << numThrds << std::endl;
}


/* this ensures all i/o happens on the same thread
* currently does not track when you unregister
* there are any pending operations
*/
void IoHandler::registerDescriptor(int fd, bool forWrite) {
    assert(started_);

    Lock lock{ handlerMutex_ };
    if (descrAssignment_.count(fd)) {
        throw std::invalid_argument("descriptor is already open");
    }
    auto it	{(forWrite) ? ioCtxts_.begin()+numReaders_: ioCtxts_.begin()};
    auto end{(forWrite) ? ioCtxts_.end() : ioCtxts_.begin() + numReaders_ };
    auto minit	=it;
    for (; it < end; ++it) {
        if ((*it)->load_ < (*minit)->load_)
            minit=it;
    }
    assert(minit != end);
    auto& ctxt=*(*minit);
    ++ctxt.load_;
    descrAssignment_.insert({fd, ctxt.id_});

}
void	IoHandler::unregisterDescriptor(int fd) {
    Lock lock{ handlerMutex_ };
    if (descrAssignment_.count(fd) == 0) {
        //can silently ignore but should not happen
        throw std::invalid_argument("descriptor is not registered");
    }
    descrAssignment_.erase({ fd});
}

size_t IoHandler::getDescrAssignment(int fd) {
    Lock lock{ handlerMutex_ };
    auto it = descrAssignment_.find(fd);
    if (it == descrAssignment_.end()) {
        //can silently ignore but should not happen
        throw std::invalid_argument("descriptor is not registered");
    }
    return it->second;
}

void	IoHandler::post(const CommandPtr& cmd) {
    size_t idx = getDescrAssignment(cmd->fd_);
    post(idx, cmd);
}

void	IoHandler::post(size_t thrdId, const CommandPtr& cmd) {
    assert(thrdId < ioCtxts_.size());
    auto& q = ioCtxts_[thrdId]->queue_;
    cmd->status_.store(CmdStatus::Pending, std::memory_order_relaxed);
    q.enqueue(cmd);
}
void IoHandler::iothrd(size_t thrdId) {

    auto& q{ioCtxts_[thrdId]->queue_};
    bool  stopped{false};
    while (!stopped) {
        auto& cmd = *q.dequeue();

        switch (cmd.op_) {

        case	CmdType::Status:
            cmd.setResult(0, 0);
            break;

        case	CmdType::Stop:
            cmd.setResult(0,0);
            stopped=true;
            break;

        case	CmdType::Read: {
            auto	 ret=fileops::read(cmd.fd_, cmd.buf_, cmd.sz_);
            if (ret > 0) {
                //this implements line mode--
                //and no error handling line has to fit in buf
                //there has to be a new line
                auto beg=((const char*)cmd.buf_);
                auto end=beg+ret-1;
                auto curr= end;
				assert(std::distance(beg,end) < std::numeric_limits<decltype(ret)>::max());
                for (; (curr >= beg) && (*curr != '\n'); --curr) NULL;
                if (curr != end) {
                    auto i=std::distance(curr,end);
                    fileops::lseek(cmd.fd_, -i, SEEK_CUR);
                    ret-=static_cast<decltype(ret)>(i);
                }

            }

            cmd.setResult(ret, (ret >= 0) ? 0: errno);

            break;
        }

        case	CmdType::Write: {
            auto ret = fileops::write(cmd.fd_, cmd.buf_, cmd.sz_);
            cmd.setResult(ret, (ret >= 0) ? 0 : errno);
            break;
        }

        }
    }
}

}
