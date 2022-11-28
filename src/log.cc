#include "log.h"
#include <stdarg.h>
#include <iostream>
#include <map>
#include <functional>
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

    LogAppender::LogAppender():level_(LogLevel::DEBUG), mutex_() {

    }
    LogAppender::~LogAppender() {

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

    LogFormatter::FormatItem::~FormatItem() {
        
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

        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
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
    /**
     * @brief 字符串的formatItem
     * 
     */
    class StringFomatItem : public LogFormatter::FormatItem {
    public:

        StringFomatItem(const std::string &str=""):str_(str) {

        }
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << str_;
        }
    private:
        std::string str_;
    };

    /**
     * @brief Level 的format item
     *   %p 日志级别
     */
    class LevelFormatItem : public LogFormatter::FormatItem {
    public:
        LevelFormatItem(const std::string &str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << LogLevel::LevelToString(level);
        }
    };

    /**
     * @brief  制表符item
     * %T 制表符
     */
    class TabFormatItem : public LogFormatter::FormatItem {
    public:
        TabFormatItem(const std::string &str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << "\t";
        }
    };
    /**
     * @brief 
     * %c 日志名称
     */
    class LogNameFormatItem : public LogFormatter::FormatItem {
    public:
        LogNameFormatItem(const std::string &str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getLogger()->getName();
        }
    };
    /**
     * @brief 
     * %t 线程id
     */
    class ThreadIdFormatItem : public LogFormatter::FormatItem {
    public:
        ThreadIdFormatItem(const std::string &str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getThreadId(); 
        }
    };
    /**
     * @brief 
     * %N 线程名称
     */
    class ThreadNameFormatItem : public LogFormatter::FormatItem {
    public:
        ThreadNameFormatItem(const std::string& str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getThreadName();
        }
    };

    /**
     * @brief 
     * %f 文件名
     */
    class FileNameFormatItem : public LogFormatter::FormatItem {
    public:
        FileNameFormatItem(const std::string& str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getFile();
        }
    };
    /**
     * @brief 
     * %l 行号
     */
    class LineFormatItem : public LogFormatter::FormatItem {
    public:
        LineFormatItem(const std::string& str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getLine();
        }
    };
    /**
     * @brief 
     * %m 消息内容
     */
    class MessageFormatItem : public LogFormatter::FormatItem {
    public:
        MessageFormatItem(const std::string& str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getContent();
        }
    };
    /**
     * @brief 
     * %n 换行
     */
    class NewLineFormatItem : public LogFormatter::FormatItem {
    public:
        NewLineFormatItem(const std::string& str="") {}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << "\n";
        }
    };
    /**
     * @brief 
     * %F 协程id
     */
    class FiberIdFormatItem : public LogFormatter::FormatItem {
    public:
        FiberIdFormatItem(const std::string& str=""){}
        void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override {
            os << event->getFiberId();
        } 
    };

    LogFormatter::LogFormatter(const std::string &pattern):pattern_(pattern) {
        init();
    }

    std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) {
        std::stringstream ss;
        for (auto &item: format_item_) {
            item->format(ss, logger, level, event);
        }
        return ss.str();
    }
    void LogAppender::setFormatter(LogFormatter::ptr formater) {
        MutexType::Lock lock(mutex_);
        formater_ = formater;
    }
    LogFormatter::ptr LogAppender::getFormater() {
        MutexType::Lock lock(mutex_);
        return formater_;
    }


    void LogFormatter::init() {
        /**
         * @brief 解析 pattern 并预定义pattern
         * 
         */


        static std::map<char, std::function<FormatItem::ptr(const std::string&)>> itemMap{
            {'d', [] (const std::string& str) { return DataTimeFormatItem::ptr(new DataTimeFormatItem(str));}},
            {'T', [] (const std::string& str) { return TabFormatItem::ptr(new TabFormatItem(str));}},
            {'p', [] (const std::string& str) { return LevelFormatItem::ptr(new LevelFormatItem(str));}},
            {'c', [] (const std::string& str) { return LogNameFormatItem::ptr(new LogNameFormatItem(str));}},
            {'t', [] (const std::string& str) { return ThreadIdFormatItem::ptr(new ThreadIdFormatItem(str));}},
            {'N', [] (const std::string& str) { return ThreadNameFormatItem::ptr(new ThreadNameFormatItem(str));}},
            {'f', [] (const std::string& str) { return FileNameFormatItem::ptr(new FileNameFormatItem(str));}},
            {'l', [] (const std::string& str) { return LineFormatItem::ptr(new LineFormatItem(str));}},
            {'m', [] (const std::string& str) { return MessageFormatItem::ptr(new MessageFormatItem(str));}},
            {'n', [] (const std::string& str) { return NewLineFormatItem::ptr(new NewLineFormatItem(str));}},
            {'F', [] (const std::string& str) { return FiberIdFormatItem::ptr(new FiberIdFormatItem(str));}}
        } ;
        std::string str;
        for (size_t i = 0; i < pattern_.size(); ++i) {
            // 不以%开头的pattern
            if (pattern_[i] != '%') {
                str.push_back(pattern_[i]);
                continue;
            }
            if (i + 1 >= pattern_.size()) {
                format_item_.push_back(StringFomatItem::ptr(new StringFomatItem("<error format %>")));
                continue;
            }
            ++i;
            // 以%开头的 pattern
            if (pattern_[i] != 'd') {
                auto it = itemMap.find(pattern_[i]);
                std::string tmp(1, pattern_[i]);
                if (it != itemMap.end()) {
                    format_item_.push_back(it->second(tmp));
                } else {
                    format_item_.push_back(StringFomatItem::ptr(new StringFomatItem("<error format %" + tmp + ">")));
                } 

            } else {
                // 解析时间

                if (i + 1 >= pattern_.size()) {
                    continue;
                } 

                if (pattern_[i+1] != '{') {
                    format_item_.push_back(itemMap['d'](""));
                    continue;
                }
                ++i;
                if (i + 1 >= pattern_.size()) {
                    format_item_.push_back(StringFomatItem::ptr(new StringFomatItem("<error format %>")));
                    continue;
                }
                ++i;
                auto index = pattern_.find_first_of('}', i);
                if (index == std::string::npos) {
                    // can not find }
                    format_item_.push_back(StringFomatItem::ptr(new StringFomatItem("<error format %d{" + pattern_.substr(i) + " >")));
                    continue;
                }
                format_item_.push_back(itemMap['d'](pattern_.substr(i, index-i)));
                i = index;
            }

        }

    }

    Logger::Logger(const std::string &logger_name)
    :level_(LogLevel::DEBUG), logger_name_(logger_name){
        formater_.reset(new LogFormatter);
    }
    Logger::~Logger() {

    }




    void Logger::Log(LogLevel::Level level, LogEvent::ptr log_event) {
        if (level >= level_) {
            Logger::ptr self = shared_from_this();
            MutexType::Lock lock(mutex_);
            if (appender_.empty()) {
                root_->Log(level, log_event);
            } else {
                for (auto& appender : appender_) {
                    appender->Log(self, level, log_event);
                }
            }


        }
    }
    void Logger::Info(LogEvent::ptr log_event) {
        Log(LogLevel::INFO, log_event);
    }
    void Logger::Debug(LogEvent::ptr log_event) {
        Log(LogLevel::DEBUG, log_event);
    }
    void Logger::Warn(LogEvent::ptr log_event) {
        Log(LogLevel::WARN, log_event);
    }
    void Logger::Error(LogEvent::ptr log_event) {
        Log(LogLevel::ERROR, log_event);
    }
    void Logger::Fatal(LogEvent::ptr log_event) {
        Log(LogLevel::FATAL, log_event);
    }

    void Logger::AddAppender(LogAppender::ptr appender) {
        if (appender) {
            appender_.push_back(appender);
        }
    }


    LoggerManager::LoggerManager() {
        root_.reset(new Logger());
        StdOutLogAppender::ptr appender = std::make_shared<StdOutLogAppender>();
        appender->setFormatter(std::make_shared<LogFormatter>());
        root_->AddAppender(appender);
        
        logger_map_["root"] = root_;
    }
    
    LoggerManager::~LoggerManager() {

    }

    Logger::ptr LoggerManager::GetInstance(const std::string& name) {
        auto it = logger_map_.find(name);
        if (it != logger_map_.end()) {
            return it->second;
        }

        Logger::ptr logger = std::make_shared<Logger>(name);
        logger->root_ = root_;
        logger_map_[name] = logger;
        return logger;
    }

    struct LogIniter {
        // Log初始化，添加相关formater
        LogIniter() {

        }
    };

}