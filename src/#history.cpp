/* 
 * history.cpp
 */
/* $begin history.cpp */
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "history.h"

history::~history() {
	while (head != nullptr) {
		node * temp = head;
		head = head->next;
		delete temp;
	}
}

std::string history::get(int idx) const {
	if (idx < 0 || idx >= size) {
		std::ostringstream oss;
		oss << "bad index " << idx;
		throw std::out_of_range(oss.str());
	}
	node * temp = head;
	for (int i = 0; i < idx; ++ i) {
		temp = temp->next;
	}
	return temp->val;
}

void history::add(const std::string & s) {
	node * pn = new node(s);
	
	if (rear == nullptr) {
		head = rear = pn;
		++ size;
		return;
	}
	
	rear->next = pn;
	rear = pn;
	
	if (isfull()) {
		node * temp = head;
		head = head->next;
		delete temp;
	} else
		++ size;
}

void show() const {
	int cnt = 1;
	node * temp = head;
	while (temp != nullptr) {
		std::cout << cnt << ": " << temp->val << std::endl;
		++ cnt;
		temp = temp->next;
	}
}

/* $end history.cpp */
