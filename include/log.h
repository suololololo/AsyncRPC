#ifndef __LOG_H__
#define __LOG_H__
#include "mutex.h"
#include "singleton.h"
#include "utils.h"
#include "thread.h"
#include <memory>
#include <sstream>
#include <list>
#include <fstream>
#include <vector>
#include <unordered_map>
#define RPC_LOG_FMT_LEVEL(logger, level, fmt, ...)\
if (logger->getLevel() <= level)\
    RPC::LogEventWrap(std::make_shared<RPC::LogEvent>(logger, level, __FILE__, __LINE__, time(0), RPC::GetThreadId(), RPC::GetFiberId(), RPC::Thread::getName())).getLogEvent()->format(fmt, __VA_ARGS__)

#define RPC_LOG_LEVEL(logger, level) \
if (logger->getLevel() <= level) \
    RPC::LogEventWrap(std::make_shared<RPC::LogEvent>(logger, level, __FILE__, __LINE__, time(0), RPC::GetThreadId(), RPC::GetFiberId(), RPC::Thread::getName() )).getSS()
#define RPC_LOG_INFO(logger) RPC_LOG_LEVEL(logger, RPC::LogLevel::INFO) 
#define RPC_LOG_DEBUG(logger) RPC_LOG_LEVEL(logger, RPC::LogLevel::DEBUG)
#define RPC_LOG_WARN(logger) RPC_LOG_LEVEL(logger, RPC::LogLevel::WARN)
#define RPC_LOG_ERROR(logger) RPC_LOG_LEVEL(logger, RPC::LogLevel::ERROR)
#define RPC_LOG_FATAL(logger) RPC_LOG_LEVEL(logger, RPC::LogLevel::FATAL)

#define RPC_LOG_FMT_DEBUG(logger, fmt, ...) RPC_LOG_FMT_LEVEL(logger, RPC::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define RPC_LOG_FMT_INFO(logger, fmt, ...) RPC_LOG_FMT_LEVEL(logger, RPC::LogLevel::INFO, fmt, __VA_ARGS__)
#define RPC_LOG_FMT_WARN(logger, fmt, ...) RPC_LOG_FMT_LEVEL(logger, RPC::LogLevel::WARN, fmt, __VA_ARGS__)
#define RPC_LOG_FMT_ERROR(logger, fmt, ...) RPC_LOG_FMT_LEVEL(logger, RPC::LogLevel::ERROR, fmt, __VA_ARGS__)
#define RPC_LOG_FMT_FATAL(logger, fmt, ...) RPC_LOG_FMT_LEVEL(logger, RPC::LogLevel::FATAL, fmt, __VA_ARGS__)
#define RPC_LOG_ROOT() RPC::LogMgr::GetInstance()->GetRoot()
#define RPC_LOG_NAME(name) RPC::LogMgr::GetInstance()->GetInstance(name)


namespace RPC {
class Logger;
/**
 * @brief 日志级别
 * 
 */
class LogLevel {
public:
    enum Level {
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        UNKNOW
    };

    static std::string LevelToString(Level level);
    static Level StringToLevel(const std::string &level);
};


/**
 * @brief 日志信息本身
 * 
 */
class LogEvent{
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, uint32_t line, uint32_t timestamp, uint32_t thread_id, uint32_t fiber_id, const std::string &thread_name);
    virtual ~LogEvent();
    const char* getFile()const {return file_;}
    uint32_t getLine() const {return line_;}
    uint32_t getTimeStamp() const {return timestamp_;}
    uint32_t getThreadId() const {return thread_id_;}
    uint32_t getFiberId() const {return fiber_id_;}
    std::stringstream& getSS() {return ss_;}
    std::string getContent() const {return ss_.str();}
    std::string getThreadName() const {return thread_name_;}
    LogLevel::Level getLevel() const {return level_;}
    std::shared_ptr<Logger> getLogger() const {return logger_;}
    
    // 日志内容格式化
    void format(const char *fmt, ...);
    void format(const char *fmt, va_list al);



private:
    std::shared_ptr<Logger> logger_;
    LogLevel::Level level_;
    const char *file_; //产生日志的文件名
    uint32_t line_;
    uint32_t timestamp_;
    uint32_t thread_id_;
    uint32_t fiber_id_;
    std::string thread_name_;
    std::stringstream ss_;

};


/**
 * @brief 日志事件封装器 负责将日志事件写入
 * 
 */
class LogEventWrap {
public:
    typedef std::shared_ptr<LogEventWrap> ptr;
    LogEventWrap(LogEvent::ptr);
    ~LogEventWrap();
    std::stringstream& getSS() { return log_event_->getSS();}
    LogEvent::ptr getLogEvent() { return log_event_;}
private:
    LogEvent::ptr log_event_;
};

/**
 * @brief 日志格式化器
 * 
 */
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    /**
     * @brief 
     * 
     * @param pattern 格式模板
     * 
     * %d 时间
     * %T 制表符
     * %p 日志级别
     * %c 日志名称
     * %t 线程id
     * %N 线程名称
     * %F 协程id
     * %f 文件名
     * %l 行号
     * %m 消息内容
     * %n 换行
     */
    explicit LogFormatter(const std::string &pattern="%d{%Y-%m-%d:%H:%M:%S}%T[ %p%T]%T[%c]%T%t%T%N%T%F%T%T%f:%l%T%m%n");
    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    /**
     * @brief 添加预定的format Item
     * 
     */
    void init();

public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem();
        virtual void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level, LogEvent::ptr event) = 0;
    };

private:
    std::string pattern_;
    std::vector<FormatItem::ptr> format_item_;
};



class Logger;
/**
 * @brief 日志输出器
 * 
 */
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;
    typedef Mutex MutexType;
    LogAppender();
    virtual ~LogAppender();

    virtual void Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    void setFormatter(LogFormatter::ptr formater);
    LogFormatter::ptr getFormater();
protected:
    LogLevel::Level level_;
    MutexType mutex_;
    LogFormatter::ptr formater_;
};
/**
 * @brief 标准输出输出器
 * 
 */
class StdOutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdOutLogAppender> ptr;
    // StdOutLogAppender();
    void Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;

private:

};

/**
 * @brief 文件输出输出器
 * 
 */
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    explicit FileLogAppender(const std::string &filename);
    void Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
    bool reopen();

private:
    std::string filename_;
    std::ofstream filestream_;
    uint64_t last_time_; // 上次修改时间

};

class LoggerManager;
/**
 * @brief 日志器
 * 
 */
class Logger : public std::enable_shared_from_this<Logger>{
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef Mutex MutexType;
    friend class LoggerManager;
    Logger(const std::string &logger_name="root");
    ~Logger();
    // 日志写入核心接口
    void Log(LogLevel::Level level, LogEvent::ptr log_event);
    void Info(LogEvent::ptr log_event);
    void Debug(LogEvent::ptr log_event);
    void Warn(LogEvent::ptr log_event);
    void Error(LogEvent::ptr log_event);
    void Fatal(LogEvent::ptr log_event);

    void AddAppender(LogAppender::ptr appender);

    std::string getName() const { return logger_name_;}
    LogLevel::Level getLevel() const { return level_;}
private: 
    std::list<LogAppender::ptr> appender_;
    LogLevel::Level level_;
    MutexType mutex_;
    Logger::ptr root_;
    std::string logger_name_;
    LogFormatter::ptr formater_;
};

/**
 * @brief Logger 管理器
 * 允许有不同得logger, 每个Logger具有不同得输出方法
 */
class LoggerManager {
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef Mutex MutexType;
    LoggerManager();
    ~LoggerManager();
    Logger::ptr GetInstance(const std::string& name);
    Logger::ptr GetRoot() {return root_;}
private:
    std::unordered_map<std::string, Logger::ptr> logger_map_;
    Logger::ptr root_;
};
using LogMgr = SingletonPtr<LoggerManager>;


}





#endif