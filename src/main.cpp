// concur.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "cmdline.h"
#include "merge.h"
#include "iohandler.h"

/*
* clean up record stream
* add line number for notice()
* executor
* testcase data generation
* not recursive does not look inside subdirectories
* need to handle glob for windows

*/

//return (std::max(std::min(getrlimit(RLIMIT_NFILE),&rlimit)
size_t getOpenFileLimit() {
	return 200;
}

size_t getWorkerLimit() {
	return 4;
}

//4MB*4=16MB*1/2*K=320GB
auto initCmdLineOptions() {
    using namespace cmdline;
    cmdline::OptionsDescription cmdlOptions;
    cmdlOptions.addOptions()
    ("datadir",				"path to location of input files: ",			".")
    ("files",				"list of input files or pattern ", ".*\\.txt",	OptionAttribute::MultiValued)
    ("maxOpenFiles",		"max number of open files",						getOpenFileLimit())
    ("maxWorkers",			"max number of merge threads",					getWorkerLimit())
    ("minFilesPerWorker",	"do this much in one thread",					getOpenFileLimit()/2)//need to play around
    ("workingDir",			"working directory:it creates a subdir multiwaymerge under this", std::filesystem::temp_directory_path().string() )
    /*("outputDir", "directory where merged file is created:default to working dir")*/
    ("outputFile",			"if filename has no path then it creates in workingDir", "MultiplexedFile.txt")
    ("recurse",				"find files in all subdirectories of data dir", false)
    /*("numericTimestamp", "treat timestamp as number", true)*/
    ("csvd",				"csv delimiter", ',')
    /*("io.async", "enable async via thread", false)*/
    ("io.nbuf",				"number of buffers per file", 4)
    ("io.bufsz",			"buffer size: cache size per file=io.nbuf*io.bufsz", 4*1024*1024)
    ("io.thrds",			"no of i/o threads split between r/w ", 4 )
    ("help", "help on options", false, OptionAttribute::StopProcessing | OptionAttribute::Switch);
    return cmdlOptions;
}

int main(int argc, const char* argv[]) {

    using namespace std;
    using namespace cmdline;

    auto	cmdlOptions{ initCmdLineOptions()};
    Parser	cmdline{cmdlOptions};

    try {
        auto args=cmdline.parse(argc,argv);
        if (args.get<bool>("help")) {
            std::cerr << cmdlOptions << std::endl;
            exit(0);
        }
        util::QuoteFileFilter qffilter{args};
        if (qffilter.files().size() == 0) {
            cerr << "no files found to merge!done" << endl;
            exit(0);
        }
        auto csvHdr= "Symbol, "+qffilter.inputCsvHeader();
        MultipassMerge::Parameters config{args, csvHdr};
        auto& files= qffilter.files();
        using symqfile::QuoteFile;
        auto& ioh=fileio::IoHandler::instance();
        auto  numIoThrds= args.get<size_t>("io.thrds");
		auto  csvd= args.get<char>("csvd");
		auto  nbufs= args.get<size_t>("io.nbuf");
		auto  bufsz = args.get<size_t>("io.bufsz");
        ioh.start(numIoThrds/2, numIoThrds / 2);
		std::cout << "using cvsDelimiter='" << csvd << "' io.nbuf=" 
				  << nbufs << " io.bufsz=" << bufsz << endl;
		symqfile::QuoteFile::delimiter(csvd);
        symqfile::QuoteFile::bufferConfig(nbufs, bufsz);
        MultipassMerge merge{ qffilter.files(),config };
        auto st=chrono::system_clock::now();
        merge.merge();
        std::chrono::duration<double, std::milli> fp_ms = chrono::system_clock::now() -st;
        cout << "elapsed time=" << fp_ms.count() << " millis" << endl;
        ioh.stop();


    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }



}

