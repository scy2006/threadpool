#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<atomic>//原子类型用来保证任务队列数量互斥
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>

class Any;
class Result;
class Task;
class semaphore;

enum PoolMode {
	MODE_FIXED,//数量固定
	MODE_CACHED,//动态增长，空闲60s自动删除，需要计时器
};

class Any {//万能类
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T data) : base_(std::make_unique<Derive<T>>(data))
	{}


	template<typename T>
	T cast_() {//获取结果
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr) {
			throw "type is wrong";
		}
		return pd->data_;
	}

private:
	//基类
	class Base {
	public:
		virtual ~Base() = default;
	};
	//派生
	template<typename T>
	class Derive :public Base {
	public:
		Derive(T data) :data_(data) {};
		T data_;
	};

private:
	std::unique_ptr<Base>base_;
	//把所有不同类型，伪装成同一种类型：Base*
};

class semaphore {//信号量
public:
	semaphore(int limit = 0) :reslimit_(limit) {
	}
	~semaphore() = default;
	//等待操作
	void wait() {
		std::unique_lock<std::mutex>lock(mtx_);
		cond_.wait(lock, [&]()->bool {return reslimit_ > 0; });
		reslimit_--;
	}
	//增加操作
	void post() {
		std::unique_lock<std::mutex>lock(mtx_);
		reslimit_++;
		cond_.notify_all();
	}
private:
	std::mutex mtx_;
	std::condition_variable cond_;
	int reslimit_;
};

class Task {
public:
	void exec();
	virtual Any run() = 0;//虚函数，这个是父类，具体任务类型通过派生实现
	void setResult(Result* res);
	Task();
	~Task() = default;
private:
	Result* result_;
};//任务类

class Result {
public:
	Result(std::shared_ptr<Task>task, bool isvalid = true);
	~Result() = default;
	void setval(Any any);

	Any get();

private:
	Any any_;//存储返回值
	semaphore sem_;//通信信号量 为0 来提示当前线程这个任务究竟完成没有
	std::shared_ptr<Task> task_;//返回任务对象
	std::atomic_bool is_valid_;
};

class  Thread {
public:
	void start();//启动该线程
	using threadhandle = std::function<void(int)>;//万能包装，这个function用来装无参数无返回的 方便跑各种函数
	Thread(threadhandle hand);
	~Thread();
	int getID()const;
private:
	threadhandle hand_;
	static int generate_id;
	int thread_id;//线程id
};//线程类  

class Threadpool {
private:
	void threadHandler(int id);

	bool check_running_state()const;
private:
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;//线程列表，键值对
	size_t init_thread_size_;//初始线程数量
	std::atomic_int idle_thread_size_;//当前空余线程数量
	int thread_size_threshhold_;//线程数量上限
	std::atomic_int cur_thread_size;//当前总数量线程

	std::queue<std::shared_ptr<Task>> taskque_;//任务队列，必须用智能指针，而且是强的，因为不清楚用户任务周期长，而且传进来用完要删除
	std::atomic_uint tasksize_;//任务数量,uint int都可以，uint表示范围大，这个又不可能为0
	size_t task_size_threshhold_;//任务队列阈值，上限

	std::mutex taskque_mtx_;//互斥锁，保证线程安全
	std::condition_variable not_full_;//任务队列非满
	std::condition_variable not_empty_; //任务队列非空
	std::condition_variable exit_cond_;//等到资源回收

	PoolMode  pool_mode_;//线程池模式

	std::atomic_bool is_pool_running_;//是否在运行
public:
	Threadpool();
	~Threadpool();
	void start(int size = std::thread::hardware_concurrency());//开启线程池
	void setMode(PoolMode mode);//设置模式
	void setTaskQueMax(int threshhold);//设置任务队列最大值
	Result submitTask(std::shared_ptr<Task> sp);//提交任务
	Threadpool(const Threadpool&) = delete;
	Threadpool& operator=(const Threadpool&) = delete;//禁止拷贝构造
	void set_thread_size_threshhold(int thresh);
};//线程池类

#endif
