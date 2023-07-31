#include "worksched.h"
#include "merge.h"
#include <sstream>
#include <algorithm>
#include <numeric>
#include <fstream>

auto	FileSizeComparator = [](auto& it1, auto it2) {
    return it1.fileSize > it2.fileSize;
};
const	MultipassMerge::Handler MultipassMerge::WorkMethod = mergeQuoteFiles;

MultipassMerge::MultipassMerge(const Files& files, const Parameters& cfg)
    :files_(files),cfg_(cfg) {
}

void MultipassMerge::merge() {
    std::cout	<< "starting merge with \n\topenFileLimit=" << cfg_.maxOpenFiles << " maxWorkers="
                << cfg_.maxWorkers << " minFilePerWorker=" << cfg_.minFilesPerWorker << std::endl;
    cleanWorkingDir();
    workScheduler_=	std::make_unique<Scheduler>(*this,
                    cfg_.maxWorkers, cfg_.minFilesPerWorker,
                    cfg_.maxOpenFiles);
    workScheduler_->run();
    workScheduler_.reset();
}

void MultipassMerge::cleanWorkingDir() {
    using namespace std::filesystem;

    auto byteCount = std::accumulate(files_.begin(), files_.end(), (size_t)0, [](auto v, auto& fi) {
        return v + fi.fileSize;
    });
    std::cout << "\tworkSet numFiles=" << files_.size() << " bytesToProcess=" << byteCount << std::endl;
	std::cout << "\tworkingDir=" << cfg_.workingDir << std::endl;
	std::cout << "\toutputFile=" << cfg_.outputFile << std::endl;
	std::cout << "\toutputCsvHdr=" << cfg_.outputCsvHeader << std::endl;
    path p{cfg_.outputFile};
	auto pexists{ std::filesystem::exists(p) };
    if (pexists) {
        if (std::filesystem::is_regular_file(p)) {
            std::cerr << "output file already exists will be removed" << std::endl;
        } else {
            throw std::invalid_argument("output file already exists and is not regular");
        }
    }

    std::ofstream ofs{p.string()};
    if (!ofs.is_open()) {
        throw std::invalid_argument("cannot create output file" + p.string());
    }
    ofs.close();
    std::filesystem::remove(p);
    std::filesystem::remove_all(cfg_.workingDir);
    std::filesystem::create_directory(cfg_.workingDir);
    /* check we can create file and disk space */
    if (std::filesystem::space(cfg_.workingDir).available < size_t(2.1 * byteCount)) {
        throw std::runtime_error("not enough disk space need "+std::to_string(size_t(2.1 * byteCount)));
    }
    std::cout << "creating merged file:" << p.string() << std::endl;

}

void  MultipassMerge::getWorkItems(size_t stage, WorkParameter& wp, WorkItems& workQ)  {
    assert(files_.size() > 0);
    if (files_.size() == 1)
        return;
    auto fileCount= workScheduler_->getTaskSize(files_.size());
    wp.outputFileName.clear();
    if (fileCount == 1) {
        wp.outputFileName=cfg_.outputFile;
    }
    wp.stage=stage;
    wp.numericTimestamp=cfg_.numericTimestamp;
    wp.outputCsvHeader=cfg_.outputCsvHeader;
    workQ=std::move(files_);
    std::sort(workQ.begin(), workQ.end(), FileSizeComparator);

}

void MultipassMerge::initRequestParameter( size_t reqId,WorkParameter& wp) const {
    if (!wp.outputFileName.empty())
        return;
    std::stringstream ss;
    ss  << "mergefile_" << std::setw(4) << std::setfill('0')
        << reqId << "_" << std::setw(3) << (wp.stage + 1) << ".txt";
    wp.outputFileName = (cfg_.workingDir / ss.str()).string();
}

void  MultipassMerge::onError(const std::exception& e)  const {
    std::cerr << "error merging files !! " << e.what() << std::endl;
    exit(0);
}

void  MultipassMerge::processCompletion(size_t stage, const WorkParameter& wp)  {
    using namespace std::filesystem;
    util::QuoteFileInfo fi{"",wp.outputFileName, file_size(path{wp.outputFileName})};

    assert(fi.fileSize > 0);
    files_.push_back(fi);
    if (stage) {
        for (auto& fi : wp.files) {
            std::filesystem::remove(path(fi.path));
        }
    }
}

