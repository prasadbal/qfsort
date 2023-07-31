#pragma once
/*
* very irritating with the initial file not having Symbol field
* trying to make both files look the same
* rec{fileIdx,symIdx, timestamp, data}
*
* not meant as a header just moved out of mergeworker to reduce
* clutter
*/
#include "symqfile.h"
#include <utility>
#include <algorithm>
#include "util.h"

inline uint64_t getTimestamp(const char* src) {
    uint64_t ts;
    static const auto HH_OFFSET = 11;
    static const auto MM_OFFSET = HH_OFFSET + 3;
    static const auto SS_OFFSET = MM_OFFSET + 3;
    static const auto MS_OFFSET = SS_OFFSET + 3;

    using TwoDigitC = util::DecimalConverter<uint64_t, 2>;
    using ThreeDigitC = util::DecimalConverter<uint64_t, 3>;
    auto  twodigc = TwoDigitC::convert;
    auto  threedigc = ThreeDigitC::convert;

    ts = ((twodigc(src + HH_OFFSET) * 100) + twodigc(src + MM_OFFSET)) * 100 + twodigc(src + SS_OFFSET);
    ts = ((ts * 1000) + threedigc(src + MS_OFFSET));
    return ts;
}


struct FieldExtractor {

    const char* pos;
    const char*	end;

    FieldExtractor(const char* line, size_t sz)
        :pos(line),end(line+sz) {
    }

    auto next()  {

        assert(pos < end); //not a true parser just extract 2 fields which should exist
        auto it	=std::find_if(pos, end, [](char c) {
            return !std::isspace(int(c));
        });
        auto it1=std::find_if(it, end, [](char c) {
            return c== symqfile::QuoteFile::delimiter();
        });
        auto sz = std::distance(it, it1);
        assert(sz > 0);
        pos = it1;
        if (pos != end) {
            assert(*pos == symqfile::QuoteFile::delimiter());
            ++pos;
        }
        return std::string_view{it, (size_t)sz};

    }
};

/*
* the idea is to do heap cmp as numeric instead of
* string cmp--so that insertion is faster
* not sure if i have the time
*/
template<size_t>
struct HeapComparator {


    void init(symqfile::QuoteRecord& r) {
        if (r.symbol().ssymbol.empty()) {
            FieldExtractor fe{ r.data(),r.size() };

            r.symbol(fe.next());
            r.timestamp(fe.next());
        }
    }

    bool operator () (const symqfile::QuoteRecord& r1, const symqfile::QuoteRecord& r2) {
        init(const_cast<symqfile::QuoteRecord&>(r1));
        init(const_cast<symqfile::QuoteRecord&>(r2));
        auto& ts1 = r1.timestamp(), & ts2 = r2.timestamp();
        if (ts1.stime > ts2.stime)
            return true;
        else if (ts1.stime == ts2.stime)
            return (r1.symbol().ssymbol > r2.symbol().ssymbol);
        return (false);

    }
};

template<>
struct HeapComparator<0> {

    void init(symqfile::QuoteRecord& r) {
        if (r.timestamp().utime == 0) {
            FieldExtractor fe{ r.data(), r.size() };
            r.timestamp(fe.next());
        }
    }

    bool operator () (const symqfile::QuoteRecord& r1, const symqfile::QuoteRecord& r2) {
        init(const_cast<symqfile::QuoteRecord&>(r1));
        init(const_cast<symqfile::QuoteRecord&>(r2));
        auto &ts1=r1.timestamp(), &ts2=r2.timestamp();
        if (ts1.stime > ts2.stime)
            return true;
        else if(ts1.stime == ts2.stime)
            return (r1.fileIndex() > r2.fileIndex());
        return (false);

    }
};
