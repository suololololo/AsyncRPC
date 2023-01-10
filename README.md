## 异步RPC框架实现

1.协程实现
协程调度模型对称调度模型，子协程的调度切换时，必须是由主协程调度，子协程执行完毕后，返回主协程

协程状态转换
![avatar](https://raw.githubusercontent.com/suololololo/AsyncRPC/master/img/fiber_std.png)


2.协程调度器实现

实现M:N的调度模型， M的线程，N个协程。
提交协程任务时可以指定线程执行，也可以不指定。

序列化模块：
1.支持以定长的方式写入基础数据类型的读写，包括int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t等等。同时用zigzag算法支持以不定长的方式写入。对于32位整数，(n<<1)^(n>>31),解码对于32位整数，(n>>1)^-(n&1)
2.支持string读写