//
// Created by tate on 17-05-20.
//

#include <iostream>
#include <thread>
#include <chrono>

#include "vm.hpp"
#include "value.hpp"
#include "bc/exec_bc_instr.hpp"
#include "global_ids.hpp"


class ExitProgramReturn : public virtual NativeFunction {
public:

	void operator()(Frame& f) override {
		// TODO: convert stack.back() to int and return it
		// f.eval_stack.back();
		if (std::holds_alternative<Value::int_t>(f.eval_stack.back().v))
			exit(std::get<Value::int_t>(f.eval_stack.back().v));
		else
			exit(0);
	}
};


VM::VM(std::vector<Literal> lit_header, const std::vector<std::string>& argv)
{
	this->literals = std::move(lit_header);

	this->main_thread = std::make_shared<Runtime>(this);
	this->main_thread->running = std::make_shared<SyncCallStack>();
	this->main_thread->running->emplace_back(std::make_shared<Frame>(
			&*this->main_thread, Closure{}));
	this->main_thread->undead.emplace_back(this->main_thread->running);
	Closure& main = this->main_thread->running->back()->closure;

	auto& entry = std::get<ClosureDef>(this->literals.back().v);
	main.body = &entry.body;
	main.i_id = entry.i_id();
	main.o_id = entry.o_id();

	// capture global variables
	for (const int64_t id : entry.capture_ids) {
		DLANG_DEBUG_MSG("capture global id # " <<id <<std::endl);
//		std::cout <<"cid" <<id <<std::endl;
		main.vars[id] = get_global_id(id);
	}

	Handle<NativeFunction> exit_fn(new ExitProgramReturn());
	main.vars[main.o_id] = Value(exit_fn);

	// TODO: capture command-line args
	Value argv_list{std::string("cmd args coming soon")};
	main.vars[main.i_id] = Value(Handle(new Handle(new Value(argv_list))));

	// declare locals
//	main.declare_empty_locals(entry.decl_ids);


}

void VM::run() {
	main_thread->run();
}

void Runtime::debugSummary() {

}

// event loop
void Runtime::run() {

	while (!this->undead.empty()) {


		// handle actions messages
		if (!this->_msg_queue.empty()) {
			std::vector<RTMessage*> msgs = this->clear_msg_queue();
			for (RTMessage *m : msgs) {
				m->action(*this);
				delete(m);
			}
		}

		// make sure we have something to do
		if (this->running == nullptr) {
			if (this->active.empty()) {
//				using namespace std::chrono_literals;
//				std::this_thread::sleep_for(1ms);
			} else {
				DLANG_DEBUG_MSG("VM:RT:Pulled Stack from active\n");
				this->running = this->active.back();
				this->active.pop_back();
			}
		} else {
			do {
				if (this->running->back()->tick()) {
					DLANG_DEBUG_MSG("VM:RT:Frame: ran out of instructions\n");
					// function ran out of instructions to run...
					// 	implicitly return value on top of stack
					std::shared_ptr<Frame>& f = this->running->back();
					Value& ret_v = f->eval_stack.back();
					Value& ret_fn = f->closure.vars[f->closure.o_id];
					if (std::holds_alternative<Value::n_fn_t>(ret_fn.v)) {
						(*std::get<Value::n_fn_t>(ret_fn.v).get_ptr())(*f);
					} else {
						this->freeze_running();
					}
					break;
				}
			} while (this->running != nullptr);

		}



	}

}