#ifndef __RPC_SERIALIZER_H__
#define __RPC_SERIALIZER_H__
#include "byte_array.h"
#include <list>
#include <set>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <type_traits>
namespace RPC {
class Serializer {
public:
    typedef std::shared_ptr<Serializer> ptr;

    Serializer() {
        byte_array_ = std::make_shared<ByteArray>();
    } 

    Serializer(ByteArray::ptr byte_array):byte_array_(byte_array) {

    }
    Serializer(const std::string &content) {
        byte_array_ = std::make_shared<ByteArray>();
        writeRowData(&content[0], content.size());
        reset();
    }
    Serializer(const char *content, size_t len) {
        byte_array_ = std::make_shared<ByteArray>();
        writeRowData(content, len);
        reset();       
    }
    ~Serializer() {

    }

    size_t getSize() { return byte_array_->getSize();}

    // size_t get
    void reset() { byte_array_->setPosition(0); }

    void offset(int off) {
        size_t old = byte_array_->getPosition();
        byte_array_->setPosition(old + off);
    }

    std::string toString() {
        return byte_array_->toString();
    }

    ByteArray::ptr getByteArray() {
        return byte_array_;
    }

    void writeRowData(const char *buf, size_t len) {
        byte_array_->write(buf, len);
    }
    /**
     * @brief 清空数据
     * 
     */
    void clear() {
        byte_array_->clear();
    }
    /**
     * @brief RPC调用过程的序列化方法
     * 
     * @tparam Args 
     * @param t 
     * @return Serializer& 
     */
    template <typename... Args>
    Serializer &operator <<(const std::tuple<Args...> &t) {
        tuple_serialize<decltype(t), sizeof...(Args)>::serialze(t, *this);
        return *this; 
    }

    template <typename... Args>
    Serializer &operator >>(std::tuple<Args...>&t) {
        tuple_deserialize<decltype(t), sizeof...(Args)>::deserialze(t, *this);
        return *this;
    }

    /**
     * @brief 核心api 反序列化各种类型的数据
     * 
     */
    template <typename T>
    void read(T &t) {
        if constexpr(std::is_same<T, bool>::value) {
            t = byte_array_->readFint8();
        } else if constexpr(std::is_same<T, float>::value) {
            t = byte_array_->readFloat();
        } else if constexpr(std::is_same<T, double>::value) {
            t = byte_array_->readDouble();
        } else if constexpr(std::is_same<T, int8_t>::value) {
            t = byte_array_->readFint8();
        } else if constexpr(std::is_same<T, uint8_t>::value) {
            t = byte_array_->readFuint8();
        } else if constexpr(std::is_same<T, int16_t>::value) {
            t = byte_array_->readFint16();
        } else if constexpr(std::is_same<T, uint16_t>::value) {
            t = byte_array_->readFuint16();
        } else if constexpr(std::is_same<T, int32_t>::value) {
            t = byte_array_->readInt32();
        } else if constexpr(std::is_same<T, uint32_t>::value) {
            t = byte_array_->readUint32();
        } else if constexpr(std::is_same<T, int64_t>::value) {
            t = byte_array_->readInt64();
        } else if constexpr(std::is_same<T, uint64_t>::value) {
            t = byte_array_->readUint64();
        } else if constexpr (std::is_same<T, std::string>::value) {
            t = byte_array_->readStringVint();
        }
    }

    /**
     * @brief 核心api 序列化各种类型的数据
     * 
     * @tparam T 
     * @param t 
     */
    template <typename T>
    void write(T t) {
        if constexpr(std::is_same<T, bool>::value) {
            byte_array_->writeFint8(t);
        } else if constexpr(std::is_same<T, float>::value) {
            byte_array_->writeFloat(t);
        } else if constexpr(std::is_same<T, double>::value) {
            byte_array_->writeDouble(t);
        } else if constexpr(std::is_same<T, int8_t>::value) {
            byte_array_->writeFint8(t);
        } else if constexpr(std::is_same<T, uint8_t>::value) {
            byte_array_->writeFuint8(t);
        } else if constexpr(std::is_same<T, int16_t>::value) {
            byte_array_->writeFint16(t);
        } else if constexpr(std::is_same<T, uint16_t>::value) {
            byte_array_->writeFuint16(t);
        } else if constexpr(std::is_same<T, int32_t>::value) {
            byte_array_->writeInt32(t);
        } else if constexpr(std::is_same<T, uint32_t>::value) {
            byte_array_->writeUint32(t);
        } else if constexpr(std::is_same<T, int64_t>::value) {
            byte_array_->writeInt64(t);
        } else if constexpr(std::is_same<T, uint64_t>::value) {
            byte_array_->writeUint64(t);
        } else if constexpr(std::is_same<T, std::string>::value) {
            byte_array_->writeStringVint(t);
        } else if constexpr(std::is_same<T, char*>::value) {
            byte_array_->writeStringVint(std::string(t));
        } else if constexpr(std::is_same<T, const char *>::value) {
            byte_array_->writeStringVint(std::string(t));
        }
    }

    template <typename T>
    Serializer &operator <<(const T&t) {
        write(t);
        return *this;
    }

    template <typename T>
    Serializer &operator >> (T &t) {
        read(t);
        return *this;
    }

    /**
     * @brief list 序列化
     * 
     * @tparam T 
     * @param v 
     * @return Serializer& 
     */
    template<typename T>
    Serializer &operator <<(const std::list<T> &v) {
        (*this) << v.size();
        for (auto & t : v) {
            (*this) << t;
        }
        return *this;
    }
    
    template<typename T>
    Serializer &operator >>(std::list<T> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            v.emplace_back(t);
        }
        return *this;
    }


    template<typename T>
    Serializer &operator <<(const std::vector<T> &v) {
        (*this) << v.size();
        for (auto &t : v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename T>
    Serializer &operator >>(std::vector<T> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            v.emplace_back(t);
        }
        return *this;
    }
    
    template<typename T>
    Serializer &operator <<(const std::set<T> &v) {
        (*this) << v.size();
        for (auto &t : v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename T>
    Serializer &operator >>(std::set<T> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            v.emplace(t);
        }
        return *this;
    }

    template<typename T>
    Serializer &operator <<(const std::multiset<T> &v) {
        (*this) << v.size();
        for (auto &t : v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename T>
    Serializer &operator >>(std::multiset<T> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            v.emplace(t);
        }
        return *this;
    }
    template<typename T>
    Serializer &operator <<(const std::unordered_set<T> &v) {
        (*this) << v.size();
        for (auto &t :v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename T>
    Serializer &operator >>(std::unordered_set<T> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            v.emplace(t);
        }
        return *this;
    }
    template<typename T>
    Serializer &operator <<(const std::unordered_multiset<T> &v) {
        (*this) << v.size();
        for (auto &t :v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename T>
    Serializer &operator >>(std::unordered_multiset<T> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            T t;
            (*this) >> t;
            v.emplace(t);
        }
        return *this;
    }
    template<typename K, typename V>
    Serializer &operator <<(const std::pair<K, V> &v) {
        (*this) << v.first << v.second;
        return *this;
    }

    template<typename K, typename V>
    Serializer &operator >>(std::pair<K, V> &v) {
        (*this) >> v.first >> v.second;
        return *this;
    }


    template<typename K, typename V>
    Serializer &operator <<(const std::map<K, V> &v) {
        (*this) << v.size();
        for (auto & t :v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename K, typename V>
    Serializer &operator >>(std::map<K, V> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> t;
            (*this) >> t;
            v.emplace(t);
        }
        return *this;
    }

    template<typename K, typename V>
    Serializer &operator <<(const std::unordered_map<K, V> &v) {
        (*this) << v.size();
        for (auto &t :v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename K, typename V>
    Serializer &operator >>(std::unordered_map<K, V> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> t;
            (*this) >> t;
            v.emplace(t);
        }
        return *this;
    }

    template<typename K, typename V>
    Serializer &operator <<(const std::multimap<K, V> &v) {
        (*this) << v.size();
        for (auto &t :v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename K, typename V>
    Serializer &operator >>(std::multimap<K, V> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> t;
            (*this) >> t;
            v.emplace(t);
        }
        return *this;
    }

    template<typename K, typename V>
    Serializer &operator <<(const std::unordered_multimap<K, V> &v) {
        (*this) << v.size();
        for (auto &t :v) {
            (*this) << t;
        }
        return *this;
    }

    template<typename K, typename V>
    Serializer &operator >>(std::unordered_multimap<K, V> &v) {
        size_t size;
        (*this) >> size;
        for (size_t i = 0; i < size; ++i) {
            std::pair<K, V> t;
            (*this) >> t;
            v.emplace(t);
        }
        return *this;
    }


private:
    /**
     * @brief 元组序列化方法
     * 
     * @tparam Tuple 
     * @tparam N 
     */
    template <typename Tuple, size_t N>
    struct tuple_serialize{
        static void serialze(const Tuple& t, Serializer &ser) {
                tuple_serialize<Tuple, N-1>::serialze(t, ser);
                ser << std::get<N-1>(t);
        }
    };
    /**
     * @brief 类模板偏特化,偏特化为元组只有一个元素的情况
     * 
     * @tparam Tuple 
     */
    template <typename Tuple>
    struct tuple_serialize<Tuple, 1>{
        static void serialze(const Tuple& t, Serializer &ser) {
            ser << std::get<0>(t);
        }
    };

    /**
     * @brief 元组反序列化方法
     * 
     * @tparam Tuple 
     * @tparam N 
     */
    template <typename Tuple, size_t N>
    struct tuple_deserialize{
        static void deserialze(const Tuple& t, Serializer &ser) {
                tuple_deserialize<Tuple, N-1>::deserialze(t, ser);
                ser >> std::get<N-1>(t);
        }
    };
    /**
     * @brief 类模板偏特化,偏特化为元组只有一个元素的情况
     * 
     * @tparam Tuple 
     */
    template <typename Tuple>
    struct tuple_deserialize<Tuple, 1>{
        static void deserialze(const Tuple& t, Serializer &ser) {
            ser >> std::get<0>(t);
        }
    };


private:
    ByteArray::ptr byte_array_;

};


};


#endif