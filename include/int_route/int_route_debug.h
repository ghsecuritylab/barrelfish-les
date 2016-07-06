/*
 * int_route_debug.h
 *
 *  Created on: Jul 5, 2016
 *      Author: luki
 */

#ifndef INT_ROUTE_INT_ROUTE_DEBUG_H_
#define INT_ROUTE_INT_ROUTE_DEBUG_H_

#define INT_ROUTE_SERVICE_DEBUG 1

#if defined(INT_ROUTE_SERVICE_DEBUG)
#define INT_DEBUG(x...) printf("int_route_service: " x)
#else
#define INT_DEBUG(x...) ((void)0)
#endif


#endif /* INCLUDE_INT_ROUTE_INT_ROUTE_DEBUG_H_ */
