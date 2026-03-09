#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef USE_DEBUG
#include "SEGGER_RTT.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_DEBUG

#define DEBUG_INIT( )	\
	do { \
		__disable_irq( );	\
		SEGGER_RTT_Init( );	\
		StackFill( );	\
		StackCheck( );	\
	} while ( 0 );
#define DEBUG_TEXT( msg )	SEGGER_RTT_printf ( 0, msg )
#define debug_log(...)		SEGGER_RTT_printf ( 0, ##__VA_ARGS__ )
#define DEBUG_PRINT( args )	debug_log args

#else

#define DEBUG_INIT( )
#define DEBUG_TEXT( msg )
#define DEBUG_PRINT( fmt, ... )

#endif

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_H__ */
