#pragma once
/*
* symbol quotes csv file
*
*/
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "iohandler.h"

namespace symqfile {

struct QuoteRecord {	//dont worry about sizes had these as uint16_t

    QuoteRecord(const char* line, size_t sz, size_t fileIndex)
        :line_(line),sz_(static_cast<uint16_t>(sz)),
         fileIndex_(static_cast<uint16_t>(fileIndex)) {
        assert(sz < std::numeric_limits<uint16_t>::max());
        assert(fileIndex_ < std::numeric_limits<uint16_t>::max());
    }

    auto fileIndex() const {
        return fileIndex_;
    }

    auto empty() const {
        return sz_ == 0;
    }

    auto data() const {
        assert(line_ != nullptr);
        return line_;
    }

    auto size() const {
        return sz_;
    }

    const auto& timestamp() const {
        return timestamp_;
    }
    const auto& symbol() const {
        return symbol_;
    }
    void symbol(std::string_view symbol) {
        symbol_.ssymbol=symbol;
    }
    void symbol(uint64_t symbol) {
        symbol_.usymbol = symbol;
    }
    void timestamp(uint64_t ts) {
        timestamp_.utime=ts;
    }
    void timestamp(std::string_view ts) {
        timestamp_.stime = ts;
    }
  private:
    const char* line_{};
    uint16_t	sz_{};
    uint16_t	fileIndex_{ std::numeric_limits<uint16_t>::max() };
    union {
        uint64_t		 usymbol;
        std::string_view ssymbol;
    } symbol_{};
    union {
        uint64_t			utime;
        std::string_view	stime;
    } timestamp_{};


};

class LineBuffer {

  public:

    using Data = std::pair<const char*, size_t>;

    Data next();

    auto  avail() const {
        assert(curr_ <= bufend_);
        return (size_t)(bufend_ - curr_);
    }

    auto  position() const {
        assert(curr_ >= buf_);
        return (size_t)(curr_-buf_);
    }

    auto capacity() const {
        return (size_t)(bufend_-buf_);
    }

    void reset(const char* buf, size_t sz) {
        buf_	= buf;
        bufend_ = buf + sz;
        curr_	= buf;

    }

    void write(const char* buf, size_t sz) {
        assert(curr_+sz <= bufend_);
        memcpy(const_cast<char*>(curr_), buf,sz);
        curr_+=sz;
    }

    //for wrap around case
    const char* curr() const {
        return curr_;
    }
    const char* end() const {
        return bufend_;
    }

  private:
    const	char* buf_{};
    const	char* bufend_{};
    const	char* curr_{};
    size_t	lineNum_{};	//for logging&&debugging
};

class QuoteFile {
  public:
    enum class OpenFor {
        Read = 1,
        Write
    };

    QuoteFile(const std::string& p, OpenFor write,size_t fileIndex);
    ~QuoteFile();
    QuoteFile(QuoteFile&&) = default;
    QuoteFile(const QuoteFile&)=delete;

    QuoteFile& operator=(QuoteFile&&) = delete;
    QuoteFile& operator=(QuoteFile&)  = delete;
    std::string fileName() const {
        return fileName_;
    }
    QuoteRecord nextRecord();
    void		writeRecord(const char* line, size_t len, bool=false);

    bool		eof() const {
        return is_eof_;
    }
    operator bool() const {
        return !is_eof_;
    }

    static char delimiter() {
        return csvDelimiter_;
    }

    static void  delimiter(char delim) {
        csvDelimiter_=delim;
    }

    static void bufferConfig(size_t numBuffers, size_t bufferSize) {
        numBuffers_= numBuffers;
        bufferSize_= bufferSize;
    }

    void skipHeader();
  private:


    using BufPtr=std::unique_ptr<char>;
    using BufPtrs=std::vector<BufPtr>;
    using Commands=std::vector<fileio::IoHandler::CommandPtr>;
    using IoResult = fileio::Result;

    size_t		fileIndex_; //like context data used by user{in use in mergeWorker heap
    std::string	fileName_;
    int			fd_{-1};
    LineBuffer	linebuf_;	//current read

    //1-1 correspondance between command
    //and databuffer
    size_t		cmdIdx_{ 0 };
    Commands	cmds_;
    BufPtrs		buffers_;
    bool		is_eof_{ false };
    bool		is_ronly{true};

    static inline	size_t	bufferSize_{ 64*1024};
    static inline	char	csvDelimiter_{','};
    static inline	size_t	numBuffers_{2};



    void open();
    void close();
    void bufferExhausted();
    void initBuffers();
    void read();
    void write(size_t);
    IoResult waitForCompletion();//fileio::Command& cmd);
    IoResult waitForCompletion(fileio::Command& cmd);

};
}
