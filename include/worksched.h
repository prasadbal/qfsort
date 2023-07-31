#pragma once
/*
* concurrenly perform a work on a setOf WorkItems
*	limiting work to a maxNumberOfWorkItems
*	this work on WorkItems can be done in stages
*
*
*/
#include <exception>
#include <vector>
#include <future>
#include <map>
#include <chrono>
#include <iostream>
#include <cassert>
#include <algorithm>

struct StopException: public  std::exception {
};

template<typename Work>
class WorkScheduler {
  public:

    WorkScheduler( Work& work, size_t maxWorkers, size_t wlMin, size_t wlMax)
        :work_(work),
         maxWorkers_(maxWorkers), workLimitMin_(wlMin), workLimitMax_(wlMax)	{
    }

    void run() {

        while (1) {
            try {
                WorkQueue		workQ;
                WorkParameter	wp;

                work_.getWorkItems(stage_,wp, workQ);
                if (workQ.empty())
                    break;
                for(auto it=workQ.begin(); it != workQ.end();) {
                    schedule(it, workQ.end(),wp);
                    waitAny();
                }
                assert(numReqsInProgress_ == 0);
                ++stage_;
            } catch (const StopException& se) {
                std::cerr << "WorkScheduler: stop requested " << se.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "WorkScheduler: unhandled exception " << e.what() << std::endl;
            }
        }
    }

    size_t getTaskSize(size_t workSize) {

        size_t taskCount{0};
        while (workSize) {
            auto	sz{ std::min(workSize, workLimitMax_) };
            auto	nWorkers{ std::min(std::max(sz / workLimitMin_, (size_t)1),maxWorkers_ ) };

            taskCount += nWorkers;
            workSize -= std::min(workLimitMax_,workSize);
        }
        return taskCount;
    }
  private:
    using WorkItem = typename Work::WorkItem;
    using WorkParameter = typename Work::WorkParameter;
    using WorkQueue = std::vector<WorkItem>;
    using RequestId = size_t;
    using Result = std::future<RequestId>; //need to generalize for work return types :-)
    using Results = std::vector<Result>;
    using RequestMap = std::map<size_t, WorkParameter>;


    Work&		work_;

    size_t		maxWorkers_;
    size_t		workLimitMax_;
    size_t		workLimitMin_;

    size_t		stage_{ 0 };
    size_t		nextReqId{ 1 };
    size_t		numReqsInProgress_{ 0 };


    RequestMap	reqsInProgress_;
    Results		results_;


    void schedule(auto &it, auto end, const WorkParameter& wp) {
		auto	wsz{ (size_t)std::distance(it,end) };//worksize
        auto	sz{std::min(wsz, workLimitMax_)};
        auto	nWorkers{ std::min(std::max(sz /workLimitMin_, (size_t)1),maxWorkers_- results_.size()) };
        auto	workerId{0u};
        auto	itlast{it+sz};
        std::vector<WorkParameter> workers(nWorkers,wp);

        for (; it < itlast; ++it) {
            workers[workerId].add(*it);
            workerId = (workerId + 1) % workers.size();
        }
		//for info will use log()
		std::cout << "starting next batch:" << nextReqId 
				  << " nWorkers=" << workers.size() 
				  << " outstanding work=" << wsz
				  << " batchSize="		  << sz<< std::endl;
        std::for_each(workers.begin(), workers.end(),[&](auto& wp) {
            auto reqId= nextReqId;
            work_.initRequestParameter(reqId, wp);
            reqsInProgress_[nextReqId++] = wp;
            results_.push_back(std::async(std::launch::async, Work::WorkMethod, reqId,wp));
        });

    }




    void waitAny() {

        Results done;
        for (; 1;) {

            for (auto it = results_.begin(); it != results_.end(); ) {
                auto& res = *it;
                if (res.valid()) {
                    done.push_back(std::move(res));
                    it = results_.erase(it);
                } else {
                    ++it;
                }
            }
            if (!done.empty())
                break;

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        try {
            for (auto& r : done)
                processCompletion(r.get());
        } catch (const std::exception& e) {
            work_.onError(e);
        }
    }

    void processCompletion(size_t reqId) {
        auto it = reqsInProgress_.find(reqId);
        assert(it != reqsInProgress_.end());
        auto& workP = it->second;
        work_.processCompletion(stage_,workP);

    }



};
