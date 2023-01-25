/* 
 * variable.cpp
 * 
 * Note: Only local variables are supported, not environment variables.
 */
/* $begin variable.cpp */
#include <iostream>
#include "variable.h"

variable var;

variable::~variable() {
	while (head != nullptr) {
		node * temp = head;
		head = head->next;
		delete temp;
	}
}

void variable::add(const std::string & nm, const std::string & val) {
	node * temp = head;
	while (temp != nullptr) {
		if (temp->name == nm) {
			temp->value = val;
			return;
		}
		temp = temp->next;
	}
	
	node * pn = new node(nm, val);
	++ size;
	
	if (rear == nullptr) {
		head = rear = pn;
		return;
	}
	
	rear->next = pn;
	rear = pn;
}

void variable::remove(const std::string & nm) {
	node * curr_node = head;
	node * prev_node = nullptr;
	
	while (curr_node != nullptr) {
		if (curr_node->name == nm) {
			if (prev_node == nullptr) {
				head = curr_node->next;
				if (head == nullptr)
					rear = nullptr;
			} else {
				prev_node->next = curr_node->next;
				if (curr_node == rear)
					rear = prev_node;
			}
			delete curr_node;
			-- size;
			return;
		} else {
			prev_node = curr_node;
			curr_node = curr_node->next;
		}
	}
}

bool variable::get(const std::string & nm, std::string & val) const {
	node * temp = head;
	while (temp != nullptr) {
		if(temp->name == nm) {		/* if success */
			val = temp->value;
			return true;
		}
		temp = temp->next;
	}
	
	return false;	/* if failed */
}

void variable::show() const {
	node * temp = head;
	while (temp != nullptr) {
		std::cout << temp->name << '=' <<  temp->value << std::endl;
		temp = temp->next;
	}
}

/* $end variable.cpp */
