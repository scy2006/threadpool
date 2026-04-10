#include"threadpool.h"
#include<iostream>
#include<functional>
#include<thread>
const int Task_Max_Threashhold = 1024;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_IDLE_TIME = 5;//60S
Threadpool::Threadpool()//构造
	:init_thread_size_(0)
	, tasksize_(0)
	, task_size_threshhold_(Task_Max_Threashhold)
	, pool_mode_(PoolMode::MODE_FIXED)//默认是fixed模式
	,is_pool_running_(false)
	,idle_thread_size_(0)//当前空余线程数量
	, thread_size_threshhold_(THREAD_MAX_THRESHHOLD)
	, cur_thread_size(0)
{}

Threadpool::~Threadpool() {//析构
	is_pool_running_ = false;
	
	std::unique_lock<std::mutex> lock(taskque_mtx_);
	not_empty_.notify_all();//将沉睡的线程唤醒
	exit_cond_.wait(lock, [&]()->bool {return threads_.size() == 0; });//等所有线程结束
	std::cout << "本次程序运行完毕，感谢使用！o(*￣▽￣*)ブ" << std::endl;
}

void Threadpool::setMode(PoolMode mode) {//定模式
	if (check_running_state())return;
		pool_mode_ = mode;
	
}


void Threadpool::setTaskQueMax(int threshhold) {//设置任务队列最大值
	if (check_running_state())return;
	task_size_threshhold_ = threshhold;
}

Result Threadpool::submitTask(std::shared_ptr<Task> sp) {//提交任务 生产任务
	  //获取锁
	std::unique_lock<std::mutex> lock(taskque_mtx_);

	//semaphore 线程通信 等待空余
	
	if (!not_full_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskque_.size() < task_size_threshhold_; })) {
		std::cerr << "抱歉，队列已满，任务提交失败，请稍等再试" << std::endl; 
		return Result(std::move(sp), false);
	}//等待1s，否则返回失败,

	
	//if 空余 任务入队列
	taskque_.emplace(sp);
	tasksize_++;

	//notify all not_empty   这个不空啦，可以来执行任务了
	not_empty_.notify_all();//就是专门叫醒：所有因为队列空、而在 wait 等待的消费者线程
	//cached模式 任务处理比较急 小而快
	if (pool_mode_ == PoolMode::MODE_CACHED &&
		tasksize_ > idle_thread_size_ &&
		cur_thread_size < thread_size_threshhold_
		) {
		auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadHandler, this,std::placeholders::_1));//make_unque等于new加上unique_ptr 在堆上创建一个对象并返回unique指针
		int threadId = ptr->getID();
		threads_.emplace(threadId, std::move(ptr));
		std::cout << "线程不足，已创建新线程"<<std::endl;
		threads_[threadId]->start();
		idle_thread_size_++;
		cur_thread_size++;
	}

	return Result(std::move(sp), true);
}


void Threadpool::threadHandler(int id) {//处理线程函数 消费任务
	auto last = std::chrono::high_resolution_clock().now();//记录时间
	for (;;) {
		std::shared_ptr<Task> sp;
		//先拿锁
		{

			std::unique_lock<std::mutex> lock(taskque_mtx_);
			std::cout << "获取任务中:" << std::this_thread::get_id() << std::endl;

				while (taskque_.size() ==0) {//防止死锁

					if (!is_pool_running_) {
						threads_.erase(id);
						std::cout << "线程id" << std::this_thread::get_id() << "已回收" << std::endl;

						exit_cond_.notify_all();
						return;//如果仍然有任务1，就不考虑是否关闭，先处理任务，如果没有，就清除
					}

					if (pool_mode_ == PoolMode::MODE_CACHED) {
						if (std::cv_status::timeout == not_empty_.wait_for(lock, std::chrono::seconds(1))) {}
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - last);
							if (dur.count() >= THREAD_IDLE_TIME&&cur_thread_size>init_thread_size_) {//如果超时1分钟，开始回收！
								//回收
								threads_.erase(id);
								cur_thread_size--;
								idle_thread_size_--;
								std::cout << "线程id" << std::this_thread::get_id() << "已回收" << std::endl;
								return;
								//修改相关值
								//从vector移除
							}
				}
					else {
						not_empty_.wait(lock);
					}
					//等待notempty

					if (!is_pool_running_) {//线程池即将关闭
						threads_.erase(id);
						std::cout << "线程id" << std::this_thread::get_id() << "已回收" << std::endl;
						exit_cond_.notify_all();//提醒主线程，该结束了
						return;
					}
			}
			
			
				
			idle_thread_size_--;


			//取一个任务
			std::cout << "获取任务成功！:" << std::this_thread::get_id() << std::endl;
			sp=taskque_.front();
			taskque_.pop();
			tasksize_--;

			if (taskque_.size() != 0) {
				not_empty_.notify_all();//提醒别人消费者，这个还有剩余
			}
			//通知生产者已经非满
			not_full_.notify_all();

		}//释放
		 
		//负责一个任务
		if (sp!=NULL) {
			sp->exec();
		}
		last = std::chrono::high_resolution_clock().now();//更新使用时间
		idle_thread_size_++;//空闲数量恢复
	}

} 

void Threadpool::start(int initThreadsize) {//启动线程池
	//初始化线程个数，默认为4，其实可以根据cpu核心个数来改变
	is_pool_running_ = true;//启动即为true
	init_thread_size_ = initThreadsize;
	cur_thread_size = initThreadsize;
	//创建线程对象
	for (int i = 0; i < init_thread_size_; i++) {
		auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadHandler, this, std::placeholders::_1));//make_unque等于new加上unique_ptr 在堆上创建一个对象并返回unique指针
		int threadId = ptr->getID();
		threads_.emplace(threadId, std::move(ptr));
	} //这个ptr必须move，这个emplace是复制进去的，unique不许复制
	/*线程没有任务、没有队列、没有锁。
线程只是个 “打工的”。
线程池才有 “任务清单” 和 “规则”。
线程必须拿着线程池的函数、用线程池的数据，才能干活。
bind 就是把 “池的指令” 交给 “线程” 去执行。
*/
	//启动所有线程
	for (int i = 0; i < init_thread_size_; i++) {
		threads_[i]->start();//启动线程
		idle_thread_size_++;
	}
}


Task::Task():result_(NULL) {
	
}
void Task::setResult(Result* res) {
	this->result_ = res;
}

void Task::exec() {
	if (result_ != NULL) {
		result_->setval(run());
	}
}

void Thread::start() {//启动线程
	//创建线程来执行这个线程函数threadhandler
	std::thread t(hand_,thread_id);
	t.detach();//把线程分离开，不用和主线程绑定
	//std::thread 对象 t，不是线程本身。
	//	t 只是线程的 “遥控器”。
	//	detach 就是：把遥控器扔了，但电视还在播
} 
int Thread::generate_id=0;


Thread::Thread(threadhandle hand)//构造线程
	:hand_(hand)
	,thread_id(generate_id++)
{}

Thread::~Thread() {//析构线程

}

Result::Result(std::shared_ptr<Task>task, bool isvalid) :is_valid_(isvalid), task_(task) {
	task_->setResult(this);
}

Any Result::get() {
	if (is_valid_==NULL) {
		return "";
	}
	sem_.wait();//让任务阻塞
	return std::move(any_);
}

void Result::setval(Any any) {
	this->any_ = std::move(any);
	sem_.post();
}

bool Threadpool::check_running_state ()const{
	return is_pool_running_;
}

void Threadpool::set_thread_size_threshhold(int thresh) {
	if (check_running_state())return;
	if(pool_mode_==PoolMode::MODE_CACHED)	thread_size_threshhold_ = thresh;

}

int Thread::getID()const {
	return thread_id;
}
