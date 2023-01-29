## 异步RPC框架实现

### 1.协程实现
协程调度模型对称调度模型，子协程的调度切换时，必须是由主协程调度，子协程执行完毕后，返回主协程

协程状态转换
![avatar](https://raw.githubusercontent.com/suololololo/AsyncRPC/master/img/fiber_std.png)


### 2.协程调度器实现

实现M:N的调度模型， M的线程，N个协程。
提交协程任务时可以指定线程执行，也可以不指定。

### 3.序列化模块：
1.支持以定长的方式写入基础数据类型的读写，包括int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t等等。同时用zigzag算法支持以不定长的方式写入。对于32位整数，(n<<1) ^ (n>>31),解码对于32位整数，(n>>1) ^ -(n&1)
2.支持string读写
3.支持基本stl容器序列化，包括vector, list, set, map, unordered_set, unordered_map, pair, unordered_multiset,multiset,multimap,unordered_multimap。
4.RPC调用过程，将参数序列化成tuple，再传输。被调用方接收到数据时，反序列化成tuple。

### IOManager 模块

### Hook模块
基于dlsym函数实现hook机制

将IO函数改为异步逻辑
只针对socket句柄进行改造。当调用原始io函数，数据未准备好时，根据句柄是否有超时时间要求，添加条件定时器，当超时了且条件还成立时，调用定时器的回调函数。回调函数具体表现为取消对该socket句柄事件的监听，并返回出连接超时的错误。添加完计时器后，监听该socket句柄的事件，挂起协程。当事件触发时（即数据准备好了），切换回该协程读数据。