/* 
 * history.h
 */
/* $begin history.h */
#ifndef HISTORY_H_
#define HISTORY_H_

#include <string>

class history {
private:
	struct node {
		std::string val;
		node * next = nullptr;
		node() = default;
		explicit node(const std::string & s)
				: val(s) { }
	};
	
	node * head = nullptr;
	node * rear = nullptr;
	const int maxsize = 500;
	int size = 0;
public:
	history() = default;
	explicit history(int ms)
			: maxsize(ms) { }
	~history();
	history(const history & h) = delete;
	history & operator=(const history & h) = delete;
	int size() const { return size; }
	int max_size() const { return maxsize; }
	bool isfull() const { return size == maxsize }
	std::string get(int idx) const;
	void add(const std::string & s);
	void show() const;
};

#endif /* HISTORY_H_ */
/* $end history.h */
