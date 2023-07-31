#pragma once
#include <atomic>

namespace fileio {

enum class CmdStatus { Idle,Pending, Complete, Error };
enum class CmdType   { Read, Write, Stop, Status };
using Status = int;

//you cannot derive without declaration
struct Result {
    int		errnum;
    size_t	xferCount;
};

struct Command {

    Command(CmdType op, int fd, void* buf,size_t sz )
        :	op_(op),
          fd_{fd},buf_{buf},sz_{sz} {
        status_.store( CmdStatus::Idle, std::memory_order_relaxed);
    }
    bool isIdle() const {
        auto status = status_.load(std::memory_order_relaxed);
        return util::compare_enum(status, CmdStatus::Idle);
    }

    bool isPending() const {
        auto status=status_.load(std::memory_order_relaxed);
        return util::compare_enum(status,CmdStatus::Pending);
    }

    bool isComplete() const {
        auto status = status_.load(std::memory_order_relaxed);
        return util::compare_enum(status, CmdStatus::Complete);
    }

    bool isError() const {
        auto status = status_.load(std::memory_order_relaxed);
        return util::compare_enum(status, CmdStatus::Error);
    }

    const Result& result() const {
        assert (!isPending());
        return result_;
    }

    //just a quick wrapper for status & stop
    bool waitResult(int msecs) const {
		auto cnt=msecs/10+1;
		
		for (;cnt > 0; --cnt){
			if (isPending())
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

        return !isPending();
    }

    size_t bufferSize() const {
        return sz_;
    }

    void reset() {
        assert(!isPending());
        result_= {0,0};
        status_.store(CmdStatus::Idle,std::memory_order_relaxed);
    }

    void reset(void *buf, size_t sz) {
        assert(!isPending());
        result_ = { 0,0 };
        buf_=buf;
        sz_=sz;
        status_.store(CmdStatus::Idle, std::memory_order_relaxed);
    }
  private:
    friend class IoHandler;
    std::atomic<CmdStatus>	status_{CmdStatus::Idle};
    CmdType		op_;
    int			fd_;
    void*		buf_;
    size_t		sz_;
    Result		result_{0,0};

    void setResult(size_t xferCount, int errnum) {
        result_.xferCount=xferCount;
        result_.errnum = errnum;
        status_.store((errnum) ?CmdStatus::Error:CmdStatus::Complete);
    }

};

class ReadCommand : public Command {
  public:
    ReadCommand(int fd, void* buf, size_t sz)
        :Command{ CmdType::Read, fd,buf, sz}
    {}

};

class WriteCommand : public Command {
  public:
    WriteCommand(int fd, void* buf, size_t sz)
        :Command{ CmdType::Write,  fd,  buf, sz }
    {}
};
}
