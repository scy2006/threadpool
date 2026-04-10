#include<iostream>
#include"threadpool.h"
#include<chrono>
#include<thread>
/*
instruction
ThreadPool pool;
pool.start(Your core of CPU);
class Mytask: public Task{
	public:
		void run(){ //your code is writing down here}
};
pool.submitTask(std::make_shared<Mytask()>);

*/
using ULong = unsigned long long;
class Mytask : public Task {
public:
	Mytask(ULong begin, ULong end) :begin_(begin), end_(end) {}
	Any run() {
		std::cout << "线程初始tid:" << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(10));
		ULong sum = 0;
		for (ULong i = begin_; i < end_; i++)sum += i;

		std::cout << "线程结束tid:" << std::this_thread::get_id() << std::endl;
		return sum;
	}
private:
	ULong begin_;
	ULong end_;
};
int main() {
	{
		Threadpool pool;
		bool flag = true;
		int sec = 0;
		while (flag) {
			std::cout << "请决定为何模式，0为fixed模式，1为cached模式" << std::endl;
			std::cin >>sec;
			if (sec == 1 || sec == 0)flag = false;
			else std::cout << "输入非法！" << std::endl;
		}
		if (sec)pool.setMode(PoolMode::MODE_CACHED);
		else pool.setMode(PoolMode::MODE_FIXED);
		std::cout << "您的CPU核心数为" << std::thread::hardware_concurrency() << std::endl;
		flag = true;
		while (flag) {
			std::cout << "请决定线程起始数，0默认为cpu核心数，其余为自定义" << std::endl;
			std::cin >> sec;
			if (sec >= 0)flag = false;
			else std::cout << "输入非法！" << std::endl;
		}
		if (!sec)pool.start();
		else pool.start(sec);


		Result res1 = pool.submitTask(std::make_shared<Mytask>(1, 1000));
		Result res2 = pool.submitTask(std::make_shared<Mytask>(1001, 2000));
		Result res3 = pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));
		pool.submitTask(std::make_shared<Mytask>(2001, 3000));

		//ULong sum1 = res1.get().cast_<ULong>();
		//ULong sum2 = res2.get().cast_<ULong>();
		//ULong sum3 = res3.get().cast_<ULong>();
		//system("pause");
		//std::cout << "1~3000相加结果是：" << (sum1 + sum2 + sum3) << std::endl;
		system("pause");
	}
	 return 0;
} 
