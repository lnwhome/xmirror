#ifndef __DEBUGHH__
#define __DEBUGHH__

#define LOG_WARNING(...) {printf __VA_ARGS__ ;printf("\n");}
#define LOG_ERROR(...) {printf __VA_ARGS__;printf("\n");}
#define LOG_MESSAGE(...) {printf __VA_ARGS__;printf("\n");}
#define LOG_TRACE(...) /*{printf __VA_ARGS__;printf("\n");}*/
#define FUNCTRACE() ;
#endif
