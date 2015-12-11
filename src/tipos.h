/*
 * Defines.h
 *
 *  Created on: 23 de nov de 2015
 *      Author: joaquim
 */

#ifndef TIPOS_H_
#define TIPOS_H_

typedef int16_t int16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uint8_t byte;

#define delete_and_nulify(ptr) 	\
	if (ptr) { 					\
		delete ptr; 			\
		ptr = NULL;				\
	}							\


#endif /* TIPOS_H_ */
