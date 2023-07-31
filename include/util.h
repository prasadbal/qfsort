#pragma once
#include <cstdint>
#include <type_traits>
#include <string>
#include <filesystem>
#include <regex>
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <list>

namespace util {
template<typename ...T>
struct is_std_vector :std::false_type {
};

template<typename ...T>
struct is_std_vector<std::vector<T...>> :std::true_type {
};

template<typename T>
constexpr auto is_std_vector_v = is_std_vector<T>::value;


template<typename E>
std::underlying_type_t<E> to_underlying(E e) {
    return static_cast<std::underlying_type_t<E>>(e);
}

template<typename E>
bool compare_enum(E e1, E e2) {
    return to_underlying(e1) == to_underlying(e2);
}

template<typename T, size_t cnt>
struct DecimalConverter {
    static T convert(const char* num) {
        static_assert("should not be called");
    }

};

template<typename T>
struct DecimalConverter<T, 2> {
    static T convert(const char* num) {
        assert(std::isdigit(num[0]) && std::isdigit(num[1]));
        return (num[0] - '0') * T { 10 } + (num[1] - '0');
    }

};

template<typename T>
struct DecimalConverter<T, 3> {
    static T convert(const char* num) {
        assert(std::isdigit(num[0]) && std::isdigit(num[1]) && std::isdigit(num[2]));
        return (num[0] - '0') * T { 100 } + (num[1] - '0') * T { 10 } + (num[2] - '0');
    }

};



template<typename F>
void iterate_directory(const std::string& dirpath, const std::string& filePattern, F f, bool recur = false) {
    using namespace std::filesystem;

    path p{ dirpath };
    if (!std::filesystem::exists(p)) {
        throw std::runtime_error{"path: " + p.string() + " does not exist"};
    }
    directory_iterator	dit{ p };
    const std::regex	e{filePattern.empty() ? ".*" : filePattern};
    auto				filter =
    [&](auto& dentry) {
        if (dentry.is_directory() && recur) {
            iterate_directory(dentry.path().string(), filePattern, f, recur);
        }
        if (!dentry.is_regular_file())
            return;
        auto& fp = dentry.path();
        if (!std::regex_match(fp.filename().string(), e)) {
            return;
        }
        f(dentry);
    };


    std::for_each(begin(dit), end(dit), filter);

}

//get information about all
//the files to merge
struct QuoteFileInfo {
    std::string symbol;
    std::string	path;
    size_t		fileSize;

};

using QuoteFileInfos = std::vector<QuoteFileInfo>;

class QuoteFileFilter {

  public:

    template<typename Options>
    QuoteFileFilter(Options&& options) {

        auto datadir = options. template get<std::string>("datadir");
        auto files = options. template get<std::string>("files");
        auto recurse = options. template get<bool>("recurse");
        iterate_directory(datadir, files, std::ref(*this), recurse);


    }

    static std::string& inputCsvHeader();

    const auto& files() const {
        return files_;
    }



    void operator()(const std::filesystem::directory_entry& de);
  private:
    QuoteFileInfos files_;

    bool checkHeader(const auto& fp, auto& ifs);
};

//simple locked queue
template<typename QueueItem>
class PCQueueT {
    using MyType= PCQueueT<QueueItem>;
  public:
    //can allow moves maybe but this is real fast
    PCQueueT()=default;
    ~PCQueueT() = default;
    PCQueueT(const MyType&)=delete;
    PCQueueT(const MyType&&) = delete;

    MyType& operator=(const MyType&)=delete;
    MyType& operator=(const MyType&&) = delete;

    void		enqueue(const QueueItem&);
    QueueItem	dequeue();
  private:
    using QueueType = std::queue<QueueItem, std::list<QueueItem>>;
    using Mutex = std::mutex;
    using Lock = std::unique_lock<Mutex>;
    using Condvar = std::condition_variable;

    QueueType	q_;
    Mutex		qlock_;
    size_t		enqueued_{ 0 };
    size_t		dequeued_{ 0 };
    Condvar		cv_;



};

//dont want to rely on size
template<typename QueueItem>
void PCQueueT<QueueItem>::enqueue(const QueueItem& cmd) {
    bool notify{ false };
    {
        Lock lock(qlock_);
        notify = q_.empty();
        q_.push(cmd);
        ++enqueued_;
        assert(dequeued_ < enqueued_);

    }
    if (notify) {
        cv_.notify_all();
    }
}

template<typename QueueItem>
QueueItem PCQueueT<QueueItem>::dequeue() {

    Lock lock(qlock_);
    if (q_.empty()) {
        cv_.wait(lock);//, [](!q.empty());
    }

    assert(!q_.empty());
    auto item = q_.front();
    q_.pop();
    return item;
}



}
