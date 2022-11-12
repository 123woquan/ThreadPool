#pragma once
#include "pch.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <future>
#include <unordered_map>
#define DLL_EXPORT _declspec(dllexport)
#define THREADPOOL_MAX_NUM 16
class Threadpool
{
private:
	std::unordered_map<std::thread::id, std::thread*> m_dthread;
	std::deque<task> m_dtask;
	int m_icount;
	std::mutex m_mt;
	std::mutex m_mGrow;
	std::condition_variable m_cv;
	bool m_bStop;               //停止
	std::atomic<int> m_nThrNum=0; //空闲线程的数量
	std::vector<std::thread::id> m_finished_threads; //待移除thread
	std::atomic<int> m_nCount = 0;
	std::atomic<int> m_nTaskCount = 0;
public:
	Threadpool():m_bStop(false)
	{
		addThread();
	}
	Threadpool& operator =(const Threadpool &) = delete;
	Threadpool(const Threadpool &) = delete;
	~Threadpool()
	{
		m_bStop = true;
		m_cv.notify_all();
		for (auto &item : m_dthread) //
		{
			if (item.second->joinable())
			{
				item.second->join();
				
			}
			delete item.second;
		
		}
	}
	void work()
	{
		bool is_run = true;
		while (!m_bStop)
		{
			std::unique_lock<std::mutex> ul(m_mt);
			//{
				//std::lock_guard<std::mutex> lg(m_mGrow);
			for (int i=0;i< m_finished_threads.size();i++)
			{
				if (m_dthread.count(m_finished_threads[i]) > 0)
				{
					if (m_dthread[m_finished_threads[i]]->joinable())
					{
						m_dthread[m_finished_threads[i]]->join();
					}
					m_dthread.erase(m_finished_threads[i]);

					std::cout << "remove thread:" << m_finished_threads[i] << std::endl;
				}
			}
			//}
			m_finished_threads.clear();

			if (m_dtask.empty())
			{
				if (m_nThrNum > 0 && m_dthread.size() > THREADPOOL_MAX_NUM)
				{
					m_finished_threads.emplace_back(std::this_thread::get_id());
					m_nThrNum--;
					break;
				}
				m_cv.wait(ul, [this]() {return !m_dtask.empty()|| m_bStop; });
			}
			if (m_bStop)
			{
				break;
			}
			//std::cout << "task:" << m_dtask.size()<<":"<< m_nTaskCount++<< std::endl;
			auto twmp = std::move(m_dtask.front());
		
		    m_dtask.pop_front();
			ul.unlock();
			if (twmp != NULL)
			{
				m_nThrNum--;				
				twmp();
				m_nThrNum++;
			}
			
			
			
		}
		//std::cout << "thread end:"<< std::this_thread::get_id() << std::endl;
	}


	
	template<typename T, typename ... arg >
	auto add(T &&t,arg && ... args) ->std::future<decltype(t(args...))>
	{
		if (m_bStop)
		{
			throw runtime_error("add:ThreadPool is stopped.");
		}
		
		using RetType = decltype(t(args...));
		auto temp = bind(std::forward<T>(t), std::forward<arg>(args)...);
		std::shared_ptr<std::packaged_task<RetType()>> sptr = std::make_shared<std::packaged_task<RetType()>>(temp);
		
		std::unique_lock<std::mutex> lg(m_mt);
		if (m_dtask.size() > 20)
		{
			//std::cout << "add task:" << m_nThrNum << m_nCount++ << std::endl;
			addThread(1);
		}
		
		m_dtask.emplace_back([sptr]{(*sptr)(); });

		m_cv.notify_one();
		lg.unlock();
		

		return sptr->get_future();
	}
private:
	void addThread(int count =4)
	{
		if (m_bStop)
		{
			throw ("addThread:ThreadPool is stopped.");;
		}		
		//std::lock_guard<std::mutex> lg(m_mGrow);
		for (int i = 0; i < count && i < THREADPOOL_MAX_NUM; i++)
		{
			auto trd = new std::thread(&Threadpool::work, this);			
			m_dthread[trd->get_id()] = trd;		
			m_nThrNum++;
		}
		
	}


};

class ThreadPoolSingleton
{
public:
	/*static void init(int count=4)
	{
		GetInstance().addThread(count);
	}*/
	
	template<typename T,typename ...arg>
	static auto Commit(T&& func, arg&& ... args)->std::future<decltype(func(args...))>
	{
		return GetInstance().add(std::forward<T>(func), std::forward<arg>(args)...);
	}
private:
	static Threadpool& GetInstance()
	{
		static Threadpool instance;
		return instance;
	}
};

