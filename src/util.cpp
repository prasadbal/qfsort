#include "util.h"
#include <iostream>
#include <fstream>


namespace util {

std::string& QuoteFileFilter::inputCsvHeader() {
    static std::string csvHdr;
    return csvHdr;
}
void QuoteFileFilter::operator()(const std::filesystem::directory_entry& de) {
    auto& fp{ de.path() };
    std::ifstream ifs(fp.string(), std::ios::in);
    if (!ifs.is_open()) {
        std::cerr << "ignoring " << fp.string() << " cannot open for read" << std::endl;
        return;
    }

    if (!checkHeader(fp, ifs))
        return;
    /*validate timestamp format*/
    /*if unrecognized switch to string compare?*/
    auto symbol = fp.filename().stem().filename().string();
    files_.push_back({ symbol,fp.string(), (size_t)de.file_size() });

}

bool QuoteFileFilter::checkHeader(const auto& fp, auto& ifs) {
    std::string hdr;
    std::getline(ifs, hdr);
    if (hdr.empty()) {
        std::cerr << "ignoring " << fp.string() << " empty hdr in file" << std::endl;
        return false;
    }
    auto& prevHdr = QuoteFileFilter::inputCsvHeader();
    if (prevHdr.empty())
        prevHdr = hdr;
    else {
        if (hdr != prevHdr) {
            std::cerr << "ignoring " << fp.string() << " empty hdr in file" << std::endl;
            return false;
        }
    }
    return true;
}

}