//
// Created by tate on 29-05-20.
//

#ifndef SCL_DEBUG_HPP
#define SCL_DEBUG_HPP

//#define SCL_DEBUG_MODE

#ifdef SCL_DEBUG_MODE
	#define SCL_DEBUG_MSG(m) std::cout <<m;
	#define SCL_DEBUG DLANG_DEBUG_MSG
#else
	#define SCL_DEBUG_MSG(m)
#endif

#endif //SCL_DEBUG_HPP
