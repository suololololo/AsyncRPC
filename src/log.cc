#include "log.h"
#include <stdarg.h>
#include <iostream>
namespace RPC {

    std::string LogLevel::LevelToString(Level level) {
        switch (level)
        {
        case DEBUG:
            return "DEBUG";
        case INFO:
            return "INFO";
        case WARN:
            return "WARN";
        case ERROR:
            return "ERROR";
        case FATAL:
            return "FATAL";
        default:
            return "UNKOWN";
        }
    }
    LogLevel::Level LogLevel::StringToLevel(const std::string &level) {
        if (level == "DEBUG" || level == "debug") {
            return LogLevel::DEBUG;
        } else if (level == "INFO" || level == "info") {
            return LogLevel::INFO;
        } else if (level == "WARN" || level == "warn") {
            return LogLevel::WARN;
        } else if (level == "ERROR" || level == "error") {
            return LogLevel::ERROR;
        } else if (level == "FATAL" || level == "fatal") {
            return LogLevel::FATAL;
        } else {
            return LogLevel::UNKNOW;
        }
    }

    LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, uint32_t line, uint32_t timestamp, uint32_t thread_id, uint32_t fiber_id, const std::string &thread_name) 
        :logger_(logger), level_(level), file_(file), line_(line), timestamp_(timestamp), thread_id_(thread_id), fiber_id_(fiber_id), thread_name_(thread_name)
    {

    }

    LogEvent::~LogEvent() {
    }
    void LogEvent::format(const char *fmt, ...) {
        va_list va;
        va_start(va, fmt);
        format(fmt, va);
        va_end(va);
    }
    void LogEvent::format(const char *fmt, va_list va) {
        char *ptr = nullptr;
        int len = vasprintf(&ptr, fmt, va);
        if (len >= 0) {
            ss_<< std::string(ptr, len);
            free(ptr);
        }
    }



    LogEventWrap::LogEventWrap(LogEvent::ptr log_event):log_event_(log_event) {

    }
    LogEventWrap::~LogEventWrap() {
        log_event_->getLogger()->Log(log_event_->getLevel(), log_event_);
        if (log_event_->getLevel() == LogLevel::FATAL) {
            exit(EXIT_FAILURE);
        }
    }

    void StdOutLogAppender::Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
        if (level < level_) {
            return;
        }
        const char *color = "";
        switch (level) {
            case LogLevel::INFO:
                break;
            case LogLevel::DEBUG:
            // 蓝色字
                color = "\033[34m";
                break;
            case LogLevel::WARN:
                color = "\033[33m";
                break;
            case LogLevel::ERROR:
            case LogLevel::FATAL:
            //红色字
                color = "\033[31m";
                break;
            default:
                break;
        }

        MutexType::Lock lock(mutex_);
        std::cout << color << formater_->format(logger, level, event) << "\033[0m"<< std::endl;

    }
    FileLogAppender::FileLogAppender(const std::string &filename):filename_(filename) {

    }

    void FileLogAppender::Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
        if (level < level_) {
            return;
        }
        uint64_t now = time(0);
        if (now > last_time_ + 3) {
            reopen();
            last_time_ = now;
        }
        MutexType::Lock lock(mutex_);
        formater_->format(logger, level, event);


    }
    bool FileLogAppender::reopen() {
        MutexType::Lock lock(mutex_);
        if (filestream_) {
            filestream_.close();
        }
        filestream_.open(filename_);
        return !!filestream_;

    }
    /**
     * @brief 时间的format Item
     * 
     */
    class DataTimeFormatItem : public LogFormatter::FormatItem {
    public:
        DataTimeFormatItem(const std::string &pattern = "%Y-%m-%d %H:%M:%S"):pattern_(pattern) {
            if (pattern_.empty()) {
                pattern_ = "%Y-%m-%d %H:%M:%S";
            }
        }

        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level, LogEvent::ptr event) override {
            struct tm tf;
            time_t time = event->getTimeStamp();
            localtime_r(&time, &tf);
            char buf[64];
            strftime(buf, sizeof(buf), pattern_.c_str(), &tf);
            os << buf;
        }
    private:
        std::string pattern_;
    };

    LogFormatter::LogFormatter(const std::string &pattern):pattern_(pattern) {
        init();
    }

    std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
        std::stringstream ss;
        for (auto item: format_item_) {
            item->format(ss, logger, level, event);
        }
    }
    
    void LogFormatter::init() {
        /**
         * @brief 解析 pattern 并预定义pattern
         * 
         */


    }






    void Logger::Log(LogLevel::Level level, LogEvent::ptr log_event) {

    }
    void Logger::Info(LogEvent::ptr log_event) {

    }
    void Logger::Debug(LogEvent::ptr log_event) {

    }
    void Logger::Warn(LogEvent::ptr log_event) {

    }
    void Logger::Error(LogEvent::ptr log_event) {

    }
    void Logger::Fatal(LogEvent::ptr log_event) {

    }


}