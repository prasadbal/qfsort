#pragma once
#include "symqfile.h"
#include "util.h"

struct MergeWorkerParameters {
    using WorkItem = util::QuoteFileInfo;
    using Record   = symqfile::QuoteRecord;
    using Symbols  = std::vector<std::string>;

    size_t			stage{0};
    util::QuoteFileInfos	files;
    Symbols			symbols;
    std::string		outputFileName;
    std::string		outputCsvHeader;
    bool			numericTimestamp{false};


    void add(WorkItem& item) {
        files.emplace_back(std::move(item));
    }
};



size_t mergeQuoteFiles(size_t requestId, const MergeWorkerParameters& p);
