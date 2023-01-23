/* 
 * variable.h
 */
/* $begin variable.h */
#ifndef VARIABLE_H_
#define VARIABLE_H_

#include <string>

class variable {
private:
	struct node {
		std::string name;
		std::string value;
		node * next = nullptr;
		node() { }
		node(const std::string & nm, const std::string & val)
				: name(nm), value(val) { }
	};
	
	node * head = nullptr;
	node * rear = nullptr;
	int size = 0;
public:
	variable() { }
	~variable();
	variable(const variable & v) = delete;
	variable & operator=(const variable & v) = delete;
	int size() const { return size; }
	void add(const std::string & nm, const std::string & val);
	void remove(const std::string & nm);
	bool get(const std::string & nm, std::string & val) const;
	void show() const;
};

#endif /* VARIABLE_H_ */
/* $end variable.h */
