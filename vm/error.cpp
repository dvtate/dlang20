//
// Created by tate on 20/04/2021.
//

#include <fstream>
#include <iostream>

#include "value_types.hpp"
#include "vm.hpp"
#include "bc/fault_table.hpp"
#include "gc/gc.hpp"
#include "../util.hpp"
#include "literal.hpp"

#include "error.hpp"

CatchFn::CatchFn(Frame &f) {
	this->frame = f.rt->running->stack.back();
}
void CatchFn::mark() {
	this->frame->mark();
}
void CatchFn::operator()(Frame& f) {
//	if (this->frame->error_handler) {
//		// Note maybe we should warn user about multiple catch statements?
//	}
	this->frame->error_handler = f.gc_make<Value>(f.eval_stack.back());
}

static std::unordered_map<std::vector<BCInstr>*, unsigned long> macro_body_lit_map_cache;
static inline size_t get_macro_start_pos(std::vector<Literal>& lits, std::vector<BCInstr>* body) {
	// Populate map so that future calls are faster in case user does this a lot?
	if (macro_body_lit_map_cache.empty())
		for (unsigned long i = 0; i < lits.size(); i++)
			if (std::holds_alternative<ClosureDef>(lits[i].v))
				macro_body_lit_map_cache[std::get<ClosureDef>(lits[i].v).body] = i;

	const Literal& l = lits[macro_body_lit_map_cache[body]];
	return std::get<ClosureDef>(l.v).start_pos;
}





ErrorTrace::ErrorTrace(SyncCallStack& cs) {
	this->trace.reserve(cs.stack.size());
	for (const auto& f : cs.stack)
		this->trace.emplace_back(std::pair<uint64_t, std::vector<BCInstr>*>{ f->pos, f->closure.body });
}

void ErrorTrace::extend(SyncCallStack& cs) {
	std::vector<std::pair<uint64_t, std::vector<BCInstr>*>> new_trace;
	new_trace.reserve(cs.stack.size() + this->trace.size());
	for (const auto& f : cs.stack)
		new_trace.emplace_back(std::pair<uint64_t, std::vector<BCInstr>*>{ f->pos, f->closure.body });
	new_trace.insert(new_trace.end(), this->trace.begin(), this->trace.end());
	this->trace = new_trace;
}

std::string ErrorTrace::depict(Frame& f, const std::string& name, const std::string& message) {
	auto& ft = f.rt->vm->fault_table;
	if (!ft)
		ft = FaultTable::read(f.rt->vm->bytecode_source);

	VM& vm = *f.rt->vm;

	std::string ret = name + ": " + message + "\n";

	// ErrorName: Error message
	// \tat depict_invoke (file_path:line_num:line_pos)
	// \tat depict_invoke (file_path:line_num:line_pos)
	// \tat depict_invoke (file_path:line_num:line_pos)
	// ...

	// For each item in the trace...
	for (int i = this->trace.size() - 1; i >= 0; i--) {
		auto& p = this->trace[i];
		ret += "\tat ";
		auto macro_start_pos = get_macro_start_pos(vm.literals, p.second);
		auto pos = p.first + macro_start_pos;
		try {
			ret += ft->invoke_lhs.at(pos) + " ";
		} catch (...) {
			ret += "unknown ";
			std::cerr <<"fault_table_miss1(" << macro_start_pos << " " << pos <<")\n";
		}
		try {
			auto src_pos = ft->relocations.at(pos);
			auto is = std::ifstream(*src_pos.first);
			auto line_col = util::pos_to_line_offset(is, src_pos.second);
			ret += "(";
			ret += (*src_pos.first) + ":" + std::to_string(line_col.first) + ":" + std::to_string(line_col.second) + ")\n";
		} catch (...) {
			ret += "(unknown:??:?\?)\n";
			std::cerr <<"fault_table_miss2(" << macro_start_pos << " " << pos <<")\n";
		}
	}

	// Last one do line snapshot
	try {
		auto macro_start_pos = get_macro_start_pos(vm.literals, this->trace.back().second);
		auto src_pos = ft->relocations.at(this->trace.back().first + macro_start_pos);
		ret = util::show_line_pos(*src_pos.first, src_pos.second) + ' ' +  ret;
	} catch (...) {
		std::cerr <<"fault_table_miss2'(_)\n";
	}

	return ret;
}


ErrorTraceStrFn::ErrorTraceStrFn(Frame& f, Value self):
	frame(f.rt->running->stack.back()),
	trace(ErrorTrace(*f.rt->running)),
	self(self)
{}
ErrorTraceStrFn::ErrorTraceStrFn(SyncCallStack& cs, Value self):
	frame(cs.stack.back()),
	trace(cs),
	self(self)
{}


void ErrorTraceStrFn::operator()(Frame& f) {
	ValueTypes::obj_t& obj = *std::get<ValueTypes::obj_ref>(self.v);
	std::string name = obj["name"].to_string();
	std::string message = obj["message"].to_string();

	ValueTypes::str_t ret = trace.depict(f, name, message);

	// Value on the back should be empty
	//    at object.member.function (/path/to/file/scl:##line:##offset)
	//	  ...
	f.eval_stack.back() = Value(ret);
}

void ErrorTraceStrFn::mark() {
	GC::mark(this->self);
	frame->mark();
}


Value gen_error_object(const std::string& name, const std::string& message, Frame& f) {
	auto* obj = f.gc_make<ValueTypes::obj_t>();
	(*obj)["name"] = Value(name);
	(*obj)["message"] = Value(message);
	(*obj)["__str"] = Value((NativeFunction*) f.gc_make<ErrorTraceStrFn>(f, Value(obj)));
	return Value(obj);
}

Value gen_error_object(const std::string& name, const std::string& message, SyncCallStack& cs) {
	auto f = cs.stack.back();
	auto* obj = f->gc_make<ValueTypes::obj_t>();
	(*obj)["name"] = Value(name);
	(*obj)["message"] = Value(message);
	(*obj)["__str"] = Value((NativeFunction*) f->gc_make<ErrorTraceStrFn>(cs, Value(obj)));
	return Value(obj);
}
