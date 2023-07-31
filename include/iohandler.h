#pragma once
#include "util.h"
#include "iocmd.h"
#include <mutex>
#include <set>
#include <unordered_map>


namespace fileio {


class IoHandler {

  public:
    using Command= fileio::Command;
    using CommandPtr=std::shared_ptr<Command>;

    static IoHandler& instance();

    void	start(size_t numReaders, size_t numWriters);
    void	stop();

    void	registerDescriptor(int fd, bool writeOnly);
    void	unregisterDescriptor(int fd);
    void	post(const CommandPtr&);



  private:
    IoHandler()=default;
    IoHandler(const IoHandler&) = delete;
	~IoHandler();
    using CmdType= fileio::CmdType;

    class StopCommand :public Command {
      public:
        StopCommand()
            :Command{ CmdType::Stop,0,nullptr,0 } {

        }
    };

    class StatusCommand :public Command {
      public:
        StatusCommand()
            :Command{ CmdType::Status,0,nullptr,0} {

        }
    };


    using DescrMap	= std::unordered_map<int,size_t>;
    using Queue		= util::PCQueueT<CommandPtr>;
    using Mutex		= std::mutex;
    using Lock		= std::unique_lock<Mutex>;

    struct IoCtxt {
        size_t		id_;
        Queue		queue_;
        size_t		load_{0};
        std::thread	thrd_;

        IoCtxt(size_t id)
            :id_(id)
        {}
    };

    using CtxtPtr  = std::unique_ptr<IoCtxt>;
    using CtxtPtrs = std::vector<CtxtPtr>;

    Mutex		handlerMutex_;
    DescrMap	descrAssignment_;
    CtxtPtrs	ioCtxts_;

    size_t		numReaders_{0};
    size_t		numWriters_{0};
    bool		started_{false};

    void	post(size_t,const CommandPtr&);
    void	iothrd(size_t iothrdId);
    size_t  getDescrAssignment(int fd);
};

}//namespace fileio