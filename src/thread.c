#include "thread.h" 


uint8_t get_thread_id(){ 
	return cur_thread->thread_id; 
}

context_t get_cur_ctx(){
	return cur_thread->context; 
}
