
#include "symqfile.h"
#include "fileops.h"
#include "util.h"
#include <system_error>
#include <iostream>
#include <iomanip>

namespace symqfile {

QuoteFile::QuoteFile(const std::string& p, OpenFor openFor, size_t fileIndex)
    :	fileIndex_(fileIndex), fileName_{p},
      is_ronly{ util::compare_enum(openFor,OpenFor::Read)} {
    open();
    initBuffers();
}

QuoteFile::~QuoteFile() {
    if (!fileName_.empty())//empty == moved object
        close();
}

void QuoteFile::open() {
    auto flags = (!is_ronly) ? O_WRONLY | O_CREAT | O_TRUNC | O_APPEND : O_RDONLY | O_BINARY | O_SEQUENTIAL;
    auto mode = (!is_ronly) ? fileops::DefaultMode : 0;
    fd_ = fileops::open(fileName_.c_str(), flags, mode); //default mode flags
    if (fd_ == -1) {
        auto ec = std::system_category().default_error_condition(errno);
        throw std::system_error{ec.value(), ec.category()};
    }
}

void QuoteFile::initBuffers() {
    cmdIdx_=0;
    /* need to take fileSize into account not to allocate more*/
    buffers_.resize(numBuffers_);
    cmds_.resize(numBuffers_);
    auto& ioh= fileio::IoHandler::instance();
    ioh.registerDescriptor(fd_, !is_ronly);

    for (auto i = 0; i < numBuffers_; ++i) {

        buffers_[i].reset(new char[bufferSize_]);
        if (is_ronly) {

            cmds_[i].reset(new fileio::ReadCommand(fd_, buffers_[i].get(), bufferSize_));
            ioh.post(cmds_[i]);
        } else {

            cmds_[i].reset(new fileio::WriteCommand(fd_, buffers_[i].get(), 0));
        }

    }

    if (!is_ronly) {
        linebuf_.reset(buffers_[0].get(),bufferSize_);
    }
}

QuoteRecord QuoteFile::nextRecord() {
    if (is_eof_) {
        return {nullptr,0,fileIndex_};
    }
    auto [recdata, sz] = linebuf_.next();
    if (sz == 0) { //current buffer exhausted

        //maybe the line was too big depends on the config bufferSize_
        if (linebuf_.avail() == linebuf_.capacity()) {
            //line does not fit in buffer
            auto msg=std::string("line too large in file: it cannot exceed") +
                     std::to_string(BUFSIZ);
            throw std::runtime_error(msg.c_str());
        }
        auto avail= linebuf_.avail();
        bufferExhausted();
        if (linebuf_.avail() == 0) {
            assert(is_eof_);
            return { nullptr, 0, fileIndex_ };
        }
        auto nxt= linebuf_.next();
        recdata=nxt.first;
        sz=nxt.second;
    }

    return QuoteRecord{recdata,sz, fileIndex_};
}

void QuoteFile::skipHeader() {
    assert(cmdIdx_==0);
    auto res=waitForCompletion();
    linebuf_.reset(buffers_[0].get(), res.xferCount);
    linebuf_.next();
}

void QuoteFile::writeRecord(const char* buf, size_t sz, bool appendNewLine) {
    auto cnt = sz;
    char newLine = '\n';

    if (appendNewLine)
        cnt += sizeof(newLine);

    if (linebuf_.avail() < cnt) {
        write(linebuf_.position());

    }

    linebuf_.write(buf, sz);
    if (appendNewLine)
        linebuf_.write(&newLine, sizeof(newLine));

}

void QuoteFile::bufferExhausted() {
    assert(is_ronly);
    assert(linebuf_.avail()==0);
    read();
}

void QuoteFile::read() {
    auto& ioh = fileio::IoHandler::instance();
    ioh.post(cmds_[cmdIdx_]);
    cmdIdx_ = (cmdIdx_ + 1) % cmds_.size();
    auto result= waitForCompletion();
    assert(result.xferCount <= bufferSize_);
    if (result.xferCount == 0)
        is_eof_=true;
    auto& buf= buffers_[cmdIdx_];
    linebuf_.reset(buf.get(), result.xferCount);

}
void QuoteFile::write(size_t bytes) {
    auto& ioh = fileio::IoHandler::instance();
    auto& cmd=*cmds_[cmdIdx_];
    cmd.reset(buffers_[cmdIdx_].get(), bytes);
    ioh.post(cmds_[cmdIdx_]);
    cmdIdx_ = (cmdIdx_ + 1) % cmds_.size();
    auto result = waitForCompletion();
    auto& resCmd = *cmds_[cmdIdx_];
    assert(result.xferCount == resCmd.bufferSize());
    auto& buf = buffers_[cmdIdx_];
    linebuf_.reset(buf.get(), bufferSize_);

}
void QuoteFile::close() {
    auto& ioh = fileio::IoHandler::instance();
    is_eof_ = true;
    if (!is_ronly) {
        if (linebuf_.position()) {
            auto& cmd = cmds_[cmdIdx_];
            write(linebuf_.position());


        }
    }

    for (auto i = 0; i < numBuffers_; ++i) {
        if (cmds_[i]->isPending())
            waitForCompletion(*cmds_[i]);
    }
    ioh.unregisterDescriptor(fd_);
    fileops::close(fd_);
    fd_ = -1;
    linebuf_.reset(nullptr, 0);
}

QuoteFile::IoResult QuoteFile::waitForCompletion() {
    auto& cmd = *cmds_[cmdIdx_];
    return waitForCompletion(cmd);
}
QuoteFile::IoResult QuoteFile::waitForCompletion(fileio::Command& cmd) {
    if (cmd.isIdle()) {
        assert(!is_ronly);
        return {0,0};
    }
    cmd.waitResult(5000);
    if (!cmd.isComplete()) {
         throw std::runtime_error("error: read timed out for file " + fileName_);
    }

    auto result = cmd.result();
    if (result.errnum) {
        auto ec = std::system_category().default_error_condition(result.errnum);
        throw std::system_error{ec.value(), ec.category()};
    }
    return result;
}

LineBuffer::Data LineBuffer::next() {
    static auto delimit_finder = [](char c) {
        return ((c == '\r') || (c == '\n'));
    };

    if ((buf_ == nullptr) || (curr_ >= bufend_)) {
        assert(curr_ == bufend_);
        return { nullptr,0 };
    }
    auto	it{ std::find_if(curr_, bufend_, delimit_finder) };
    if (it == bufend_) {
        return { nullptr,0 };
    }

    auto	sz{ std::distance(curr_, it) };
    auto	src{ curr_ };
    curr_ = it;
    ++lineNum_;
    while ((sz == 0) && (it < bufend_)) {
        if (*it == '\r') ++it;
        if ((it < bufend_) && (*it == '\n')) ++it;
        auto it1 = std::find_if(it, bufend_, delimit_finder);
        sz = std::distance(it, it1);
        src = it;
        curr_ = it1;
        ++lineNum_;
    }


    if ((curr_ < bufend_) && (*curr_ == '\r')) ++curr_;
    if ((curr_ < bufend_) && (*curr_ == '\n')) ++curr_;
    return { src, sz };
}


}//namespace symqfile