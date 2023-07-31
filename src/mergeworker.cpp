#include "mergeworker.h"
#include "heapcmp.h"
#include <queue>
#include <iostream>


using Symbols=std::vector<std::string>;

template<size_t N>
void writeRecord(symqfile::QuoteFile& qf, symqfile::QuoteRecord& rec, const Symbols& symbols, const std::integral_constant<size_t,N>&) {
    qf.writeRecord(rec.data(), rec.size(), true);
}

/*
*	going back and forth with inputRecord should have '\n': no time for now to do it properly
*   its anyway buffered.
*/
void writeRecord(symqfile::QuoteFile& qf, symqfile::QuoteRecord& rec, const Symbols& symbols,const std::integral_constant<size_t,0>&) {
    static size_t idx1=1;
    assert(rec.fileIndex() < symbols.size());
    auto idx{ rec.fileIndex()};
    auto symbol{ symbols[idx]};
    assert(symbol.size());
    qf.writeRecord(symbol.data(),symbol.size());
    qf.writeRecord(rec.data(), rec.size(), true);

}

template<size_t stage>
size_t mergeQuoteFilesT(size_t requestId, const MergeWorkerParameters& p) {
    using symqfile::QuoteRecord;
    using symqfile::QuoteFile;
    using stage_tag = std::integral_constant<size_t, stage>;
    using HeapItem = QuoteRecord;
    using Heap = std::priority_queue<HeapItem, std::vector<HeapItem>, HeapComparator<stage> >;

    auto					 files{ p.files };
    std::vector<QuoteFile>	 inputQuoteFiles;
    QuoteFile				 outputQuoteFile{ p.outputFileName, QuoteFile::OpenFor::Write, 0 };
    Symbols					 symbols{};

    outputQuoteFile.writeRecord(p.outputCsvHeader.data(), p.outputCsvHeader.size(),true);
    if constexpr (stage == 0) {
        /* --lets re-sort by symbol & not by size, so that we can use fileIndex as symbol for comparision*/
        std::sort(files.begin(), files.end(), [](auto& f1, auto& f2) {
            return f1.symbol < f2.symbol;
        });
        std::for_each(files.begin(), files.end(), [&symbols](auto& fi) {
            symbols.push_back(fi.symbol+", ");
        });
    }

    std::for_each(files.begin(), files.end(),
    [&](const auto& fi) {

        inputQuoteFiles.emplace_back(
            std::move(QuoteFile{ fi.path,QuoteFile::OpenFor::Read, inputQuoteFiles.size() })
        );

    });

    //its better to do here instead of automatically as all the files
    //can issue aysnc read requests
    std::for_each(inputQuoteFiles.begin(), inputQuoteFiles.end(),[](auto &qf) {
        qf.skipHeader();
    });

    Heap heap;
    for (auto& iqf : inputQuoteFiles) {
        auto rec = HeapItem{ iqf.nextRecord() };
        if (!rec.empty())
            heap.push(rec);
    }

    while (!heap.empty()) {
        auto rec = heap.top();
        heap.pop();
        writeRecord(outputQuoteFile, rec, symbols, stage_tag{});

        auto nextrec = HeapItem{ inputQuoteFiles[rec.fileIndex()].nextRecord() };
        if (!nextrec.empty())
            heap.push(nextrec);
    }

    return requestId;
}


size_t mergeQuoteFiles(size_t requestId, const MergeWorkerParameters& p) {

    return (p.stage ? mergeQuoteFilesT<1>(requestId, p) : mergeQuoteFilesT<0>(requestId, p));

}