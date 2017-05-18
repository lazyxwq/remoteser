/*
 * tqueue.h
 *
 *  Created on: May 18, 2017
 *      Author: xwq
 */

#ifndef TQUEUE_H_
#define TQUEUE_H_

/** * @author  作者 E-mail: 
 * @date 创建时间：May 18, 2017 8:22:10 AM 
 * @version 1.0 
 * @parameter  
 * @since  
 * @return  
 */
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <iostream>

template<class T>
class t_queue {
private:
	mutable std::mutex mut;
	std::queue<T> data_queue;
	std::condition_variable data_con;
public:
	t_queue() {
	}
	t_queue(t_queue const& other) {
		std::lock_guard<std::mutex> lk(other.mut);
		data_queue = other.data_queue;
	}
	void push(T tValue) {
		std::lock_guard<std::mutex> lk(mut);
		data_queue.push(tValue);
		data_con.notify_one();
	}
	void wait_and_pop(T& tValue) {
		std::unique_lock<std::mutex> lk(mut);
		data_con.wait(lk, [this] {return !data_queue.empty();});
		tValue = data_queue.front();
		data_queue.pop();
	}
	std::shared_ptr<T> wait_and_pop() {
		std::unique_lock<std::mutex> lk(mut);
		data_con.wait(lk, [this] {return !data_queue.empty();});
		std::shared_ptr<T> ret(std::make_shared<T>(data_queue.front()));
		data_queue.pop();
		return ret;
	}
	bool try_pop(T& tValue) {
		std::lock_guard<std::mutex> lk(mut);
		if (data_queue.empty())
			return false;
		tValue = data_queue.front();
		data_queue.pop();
		return true;
	}

	std::shared_ptr<T> try_pop() {
		std::lock_guard<std::mutex> lk(mut);
		if (data_queue.empty())
			return std::shared_ptr<T>();
		std::shared_ptr<T> ret(std::make_shared(data_queue.front()));
		data_queue.pop();
		return ret;
	}

	bool empty() const {
		std::lock_guard<std::mutex> lk(mut);
		return data_queue.empty();
	}
};

#endif /* TQUEUE_H_ */
