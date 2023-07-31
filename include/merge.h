#pragma once
#include "worksched.h"
#include "symqfile.h"
#include "mergeworker.h"
#include "util.h"
#include <map>
#include <sstream>
#include <cassert>


class MultipassMerge {
  public:

    class Parameters {
      public:
        template<typename Options>
        Parameters(Options&& values, const std::string& outcsv) {
            using std::filesystem::path;
            maxOpenFiles = values.template get<size_t>("maxOpenFiles");
            maxWorkers = values.template get<size_t>("maxWorkers");
            minFilesPerWorker = values.template get<size_t>("minFilesPerWorker");
            auto wd= path(values.template get<std::string>("workingDir"));
            //wd= std::filesystem::absolute(wd);
            workingDir =wd/ "multiwaymerge";
            outputCsvHeader = outcsv;
            numericTimestamp=false;
            path outpath{values.template get<std::string>("outputFile")};
            if (!outpath.has_parent_path()) {
                outpath =wd/ outpath;
            }
            outputFile= outpath.string();
            /*numericTimestamp = values.template get<bool>("numericTimestamp");*/


        }

        Parameters(const Parameters&)=default;

      private:
        using path=std::filesystem::path;
        size_t		maxOpenFiles;
        size_t		maxWorkers;
        size_t		minFilesPerWorker;
        //tempDir called working because we could try and restart from last failure
        path		workingDir;
        std::string outputFile;
        std::string	outputCsvHeader;
        bool		numericTimestamp;

        friend class MultipassMerge;

    };

    using Files		= util::QuoteFileInfos;
    using WorkItem	= util::QuoteFileInfo;
    using WorkItems = std::vector<WorkItem>;
    using WorkParameter = MergeWorkerParameters;
    using Handler = size_t(*)(size_t, const MergeWorkerParameters&);
    static const Handler WorkMethod;



    MultipassMerge(const Files& files, const Parameters& cfg);


    void merge();
  private:
    using Scheduler= WorkScheduler<MultipassMerge>;
    using SchedulerPtr=std::unique_ptr<Scheduler>;
    friend Scheduler;

    Parameters		cfg_;
    Files			files_; //to merge
    SchedulerPtr	workScheduler_;


    void getWorkItems(size_t stage, WorkParameter& wp, WorkItems& workQ) ;
    void initRequestParameter(size_t reqId, WorkParameter& wp) const;
    void onError(const std::exception&) const ;
    void processCompletion(size_t stage, const WorkParameter&) ;
    void cleanWorkingDir();


};
